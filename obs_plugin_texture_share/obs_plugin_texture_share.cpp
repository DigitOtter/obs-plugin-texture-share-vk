#include "obs_plugin_texture_share/obs_plugin_texture_share.hpp"

#include <obs.h>
#include <obs/graphics/graphics.h>
#include <texture_share_vk/opengl/texture_share_gl_client.h>
#include <util/c99defs.h>


extern "C"
{
	/* Required OBS module commands (required) */
	OBS_DECLARE_MODULE();
	OBS_MODULE_USE_DEFAULT_LOCALE(OBSPluginTextureShare::PLUGIN_NAME.data(), "en-US");

	// Define plugin info
	const char *obs_get_name(void *type_data);
	void       *obs_create(obs_data_t *settings, obs_source_t *source);
	void        obs_destroy(void *data);
	// uint32_t    obs_get_width(void *data);
	// uint32_t    obs_get_height(void *data);
	void obs_update(void *data, obs_data_t *settings);
	void obs_video_render(void *data, gs_effect_t *effect);
	void obs_offscreen_render(void *param, uint32_t cx, uint32_t cy);

	constexpr struct obs_source_info obs_plugin_texture_share_info = {
		.id           = "my_source",
		.type         = OBS_SOURCE_TYPE_FILTER,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name     = obs_get_name,
		.create       = obs_create,
		.destroy      = obs_destroy,
		// .get_width    = obs_get_width,
	    // .get_height   = obs_get_height,
		.update       = obs_update,
		.video_render = obs_video_render,
	};

	// Load module
	bool obs_module_load(void)
	{
		obs_register_source(&obs_plugin_texture_share_info);
		return true;
	}

	const char *obs_get_name(void *type_data)
	{
		UNUSED_PARAMETER(type_data);
		return OBSPluginTextureShare::PLUGIN_NAME.data();
	}

	void *obs_create(obs_data_t *settings, obs_source_t *source)
	{
		return new OBSPluginTextureShare(settings, source);
	}

	void obs_destroy(void *data)
	{
		if(data)
			delete reinterpret_cast<OBSPluginTextureShare *>(data);
	}

	void obs_update(void *data, obs_data_t *settings)
	{
		return reinterpret_cast<OBSPluginTextureShare *>(data)->Update(settings);
	}

	void obs_video_render(void *data, gs_effect_t *effect)
	{
		return reinterpret_cast<OBSPluginTextureShare *>(data)->Render(effect);
	}

	void obs_offscreen_render(void *param, uint32_t cx, uint32_t cy)
	{
		return reinterpret_cast<OBSPluginTextureShare *>(param)->OffscreenRender(cx, cy);
	}
}

OBSPluginTextureShare::OBSPluginTextureShare(obs_data_t *settings, obs_source_t *source)
{
	this->_source = obs_source_get_ref(source);
	obs_add_main_render_callback(obs_offscreen_render, this);
}

OBSPluginTextureShare::~OBSPluginTextureShare()
{
	const auto lock         = std::lock_guard(this->_access);
	this->_update_available = false;

	obs_remove_main_render_callback(obs_offscreen_render, this);

	if(this->_render_target)
	{
		gs_texrender_destroy(this->_render_target);
		this->_render_target = nullptr;
	}

	if(this->_source)
	{
		obs_source_release(this->_source);
		this->_source = nullptr;
	}
}

void OBSPluginTextureShare::Render(gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	{
		const auto lock = std::lock_guard(this->_access);
		// Mark available update for offscreen rendering
		this->_update_available = true;
	}

	// No filter, only offscreen rendering
	obs_source_skip_video_filter(this->_source);
}

void OBSPluginTextureShare::OffscreenRender(uint32_t cx, uint32_t cy)
{
	// Wait for update
	if(!this->_update_available)
		return;

	const auto lock         = std::lock_guard(this->_access);
	this->_update_available = false;

	const uint32_t width  = obs_source_get_base_width(this->_source);
	const uint32_t height = obs_source_get_base_height(this->_source);

	// Init offscreen render target
	if(!this->_render_target)
	{
		if(!this->UpdateRenderTarget(width, height))
			return;
	}

	// Perform offscreen rendering
	gs_texrender_reset(this->_render_target);
	if(gs_texrender_begin(this->_render_target, width, height))
	{
		struct vec4 background;
		vec4_zero(&background);

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_video_render(this->_source);

		gs_blend_state_pop();
		gs_texrender_end(this->_render_target);
	}

	// Send to shared texture
	gs_texture_t *const ptex       = gs_texrender_get_texture(this->_render_target);
	GLuint *const       gl_texture = reinterpret_cast<GLuint *>(gs_texture_get_obj(ptex));
	if(gl_texture)
	{
		// Texture size
		const TextureShareGlClient::ImageExtent image_size{
			{0,              0              },
			{(GLsizei)width, (GLsizei)height},
		};

		// Send texture
		this->_tex_share_gl.SendImageBlit("image_name", *gl_texture, GL_TEXTURE_2D, image_size);
	}
}

bool OBSPluginTextureShare::UpdateRenderTarget(uint32_t width, uint32_t height)
{
	if(this->_render_target)
		gs_texrender_destroy(this->_render_target);

	this->_render_target = gs_texrender_create(GS_BGRA, GS_ZS_NONE);

	// Initialize image.
	this->_tex_share_gl.InitImage("image_name", width, height, true);

	return true;
}
