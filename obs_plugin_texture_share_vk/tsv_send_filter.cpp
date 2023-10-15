#include "tsv_send_filter.hpp"

#include <obs.h>
#include <obs/graphics/graphics.h>


extern "C"
{
    /* Required OBS module commands (required) */
    OBS_DECLARE_MODULE();
	OBS_MODULE_USE_DEFAULT_LOCALE(OBSPluginTextureShareFilter::PLUGIN_NAME.data(), "en-US");

	// Define plugin info
	const char *obs_get_name(void *type_data);
	void       *obs_create(obs_data_t *settings, obs_source_t *source);
	void        obs_destroy(void *data);
	// uint32_t    obs_get_width(void *data);
	// uint32_t    obs_get_height(void *data);
	void              obs_get_defaults(void *data, obs_data_t *defaults);
	obs_properties_t *obs_get_properties(void *data);
	void              obs_update(void *data, obs_data_t *settings);
	void obs_video_render(void *data, gs_effect_t *effect);
	void obs_offscreen_render(void *param, uint32_t cx, uint32_t cy);

	constexpr struct obs_source_info obs_plugin_shared_texture_filter_info = {
		.id             = TsvSendFilter::PLUGIN_NAME.data(),
		.type           = OBS_SOURCE_TYPE_FILTER,
		.output_flags   = OBS_SOURCE_VIDEO,
		.get_name       = obs_get_name,
		.create         = obs_create,
		.destroy        = obs_destroy,
		.get_properties = obs_get_properties,
		.update         = obs_update,
		.video_render   = obs_video_render,
		.get_defaults2  = obs_get_defaults,
	};

	// Load module
	bool obs_module_load(void)
	{
		TextureShareGlClient::initialize_gl_external();
		obs_register_source(&obs_plugin_shared_texture_filter_info);
		return true;
	}

	const char *obs_get_name(void *type_data)
	{
		UNUSED_PARAMETER(type_data);
		return TsvSendFilter::PLUGIN_NAME.data();
	}

	void *obs_create(obs_data_t *settings, obs_source_t *source)
	{
		void *data = bmalloc(sizeof(TsvSendFilter));
		new(data) TsvSendFilter(settings, source);
		return data;
	}

	void obs_destroy(void *data)
	{
		if(data)
		{
			reinterpret_cast<TsvSendFilter *>(data)->~TsvSendFilter();
			bfree(data);
		}
	}

	void obs_get_defaults(void *data, obs_data_t *defaults)
	{
		return reinterpret_cast<TsvSendFilter *>(data)->GetDefaults(defaults);
	}

	obs_properties_t *obs_get_properties(void *data)
	{
		return reinterpret_cast<TsvSendFilter *>(data)->GetProperties();
	}

	void obs_update(void *data, obs_data_t *settings)
	{
		return reinterpret_cast<TsvSendFilter *>(data)->UpdateProperties(settings);
	}

	void obs_video_render(void *data, gs_effect_t *effect)
	{
		return reinterpret_cast<TsvSendFilter *>(data)->Render(effect);
	}

	void obs_offscreen_render(void *param, uint32_t cx, uint32_t cy)
	{
		return reinterpret_cast<TsvSendFilter *>(param)->OffscreenRender(cx, cy);
	}
}

TsvSendFilter::TsvSendFilter(obs_data_t *settings, obs_source_t *source)
	: _source(obs_source_get_ref(source))
{
	this->UpdateSharedTextureName(settings);

	obs_add_main_render_callback(obs_offscreen_render, this);

	this->_tex_share_gl.init_with_server_launch();
}

TsvSendFilter::~TsvSendFilter()
{
	// Note: Enter graphics first to prevent race condition
	obs_enter_graphics();
	const auto lock         = std::lock_guard(this->_access);
	this->_render_state     = WAITING;

	obs_remove_main_render_callback(obs_offscreen_render, this);

	if(this->_render_target)
	{
		obs_enter_graphics();
		gs_texrender_destroy(this->_render_target);
		this->_render_target = nullptr;
		obs_leave_graphics();
	}

	if(this->_source)
	{
		obs_source_release(this->_source);
		this->_source = nullptr;
	}

	obs_leave_graphics();
}

obs_properties_t *TsvSendFilter::GetProperties()
{
	obs_properties_t *properties = obs_properties_create();

	obs_properties_add_text(properties, PROPERTY_SHARED_TEXTURE_NAME.data(),
	                        obs_module_text(PROPERTY_SHARED_TEXTURE_NAME.data()), OBS_TEXT_DEFAULT);

	obs_properties_add_button(properties, PROPERTY_APPLY_BUTTON.data(), obs_module_text(PROPERTY_APPLY_BUTTON.data()),
	                          &TsvSendFilter::PropertyClickedCb);

	return properties;
}

void TsvSendFilter::GetDefaults(obs_data_t *defaults)
{
	obs_data_set_default_string(defaults, PROPERTY_SHARED_TEXTURE_NAME.data(),
	                            obs_module_text(PROPERTY_SHARED_TEXTURE_NAME_DEFAULT.data()));
}

void TsvSendFilter::UpdateProperties(obs_data_t * /*settings*/)
{
	// Do nothing. Only update settings when apply button is pressed.
	// This prevents creating multiple shared textures when typing in the name text field
}

void TsvSendFilter::Render(gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	if(this->_render_state != OFFSCREEN_RENDERING)
	{
		const auto lock = std::lock_guard(this->_access);
		// Mark available update for offscreen rendering
		this->_render_state = UPDATE_AVAILABLE;
	}

	// No filter, only offscreen rendering
	obs_source_skip_video_filter(this->_source);
}

void TsvSendFilter::OffscreenRender(uint32_t /*cx*/, uint32_t /*cy*/)
{
	// Wait for update
	if(this->_render_state != UPDATE_AVAILABLE)
		return;

	const auto lock         = std::lock_guard(this->_access);
	this->_render_state     = WAITING;

	const uint32_t width  = obs_source_get_base_width(this->_source);
	const uint32_t height = obs_source_get_base_height(this->_source);

	// Init offscreen render target
	if(!this->_render_target || width != this->_tex_width || height != this->_tex_height)
	{
		if(!this->UpdateRenderTarget(width, height))
			return;
	}

	// Perform offscreen rendering
	gs_texrender_reset(this->_render_target);
	if(gs_texrender_begin(this->_render_target, width, height))
	{
		// obs_source_video_render() calls this->Render(). This prevents a deadlock
		this->_render_state = OFFSCREEN_RENDERING;

		struct vec4 background;
		vec4_zero(&background);
		background.x = 1.0f;

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_video_render(this->_source);

		gs_blend_state_pop();
		gs_texrender_end(this->_render_target);

		this->_render_state = WAITING;

		// Send to shared texture
		gs_texture_t *const ptex       = gs_texrender_get_texture(this->_render_target);
		GLuint *const       gl_texture = reinterpret_cast<GLuint *>(gs_texture_get_obj(ptex));
		if(gl_texture)
		{
			// Texture size
			const ImageExtent image_size{
				{0,              0              },
				{(GLsizei)width, (GLsizei)height},
			};

			// Send texture
			GLint drawFboId = 0;
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);

			this->_tex_share_gl.send_image(this->_shared_texture_name.c_str(), *gl_texture, GL_TEXTURE_2D, false,
			                               drawFboId, &image_size);
		}
	}
}

bool TsvSendFilter::UpdateRenderTarget(uint32_t width, uint32_t height)
{
	if(width == 0 || height == 0)
		return false;

	obs_enter_graphics();

	if(!this->_render_target)
		this->_render_target = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

	// Initialize image. (OBS's GS_BGRA should translate to OpenGL's GL_BGRA)
	this->_tex_share_gl.init_image(this->_shared_texture_name.c_str(), width, height, ImgFormat::R8G8B8A8, true);

	this->_tex_width  = width;
	this->_tex_height = height;

	obs_leave_graphics();

	return true;
}

void TsvSendFilter::UpdateSharedTextureName(obs_data_t *settings)
{
	// Check if sender name was updated
	const char *new_sender_name = obs_data_get_string(settings, PROPERTY_SHARED_TEXTURE_NAME.data());
	if(this->_shared_texture_name != new_sender_name)
	{
		// If name was updated, reinitialize render target
		// Note: Enter graphics first to prevent race condition
		obs_enter_graphics();
		const auto lock = std::lock_guard(this->_access);
		if(this->_render_target)
		{
			gs_texrender_destroy(this->_render_target);
			this->_render_target = nullptr;
		}

		obs_leave_graphics();

		this->_shared_texture_name = obs_data_get_string(settings, PROPERTY_SHARED_TEXTURE_NAME.data());
	}
}

bool TsvSendFilter::PropertyClickedCb(obs_properties_t *props, obs_property_t *property, void *data)
{
	return reinterpret_cast<TsvSendFilter *>(data)->PropertyClicked(props, property);
}

bool TsvSendFilter::PropertyClicked(obs_properties_t * /*props*/, obs_property_t * /*property*/)
{
	obs_data_t *settings = obs_source_get_settings(this->_source);

	// Update shared texture name
	this->UpdateSharedTextureName(settings);

	obs_data_release(settings);
	return true;
}
