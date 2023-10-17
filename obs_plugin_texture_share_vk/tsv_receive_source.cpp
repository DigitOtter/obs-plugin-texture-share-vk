#include "tsv_receive_source.hpp"

#include <obs.h>
#include <obs/graphics/graphics.h>


extern "C"
{
    /* Required OBS module commands (required) */
    OBS_DECLARE_MODULE();
	OBS_MODULE_USE_DEFAULT_LOCALE(OBSPluginTextureShareSource::PLUGIN_NAME.data(), "en-US");

	// Define plugin info
	const char       *obs_get_name(void *type_data);
	void             *obs_create(obs_data_t *settings, obs_source_t *source);
	void              obs_destroy(void *data);
	uint32_t          obs_get_width(void *data);
	uint32_t          obs_get_height(void *data);
	void              obs_get_defaults(void *data, obs_data_t *defaults);
	obs_properties_t *obs_get_properties(void *data);
	void              obs_update(void *data, obs_data_t *settings);
	void              obs_video_tick(void *data, float seconds);
	void              obs_video_render(void *data, gs_effect_t *effect);

	constexpr struct obs_source_info obs_plugin_texture_share_info = {
		.id             = TsvReceiveSource::PLUGIN_NAME.data(),
		.type           = OBS_SOURCE_TYPE_INPUT,
		.output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
		.get_name       = obs_get_name,
		.create         = obs_create,
		.destroy        = obs_destroy,
		.get_width      = obs_get_width,
		.get_height     = obs_get_height,
		.get_properties = obs_get_properties,
		.update         = obs_update,
		.video_tick     = obs_video_tick,
		.video_render   = obs_video_render,
		.get_defaults2  = obs_get_defaults,
	};

	// Load module
	bool obs_module_load(void)
	{
		TextureShareGlClient::initialize_gl_external();
		obs_register_source(&obs_plugin_texture_share_info);
		return true;
	}

	const char *obs_get_name(void *type_data)
	{
		UNUSED_PARAMETER(type_data);
		return TsvReceiveSource::PLUGIN_NAME.data();
	}

	void *obs_create(obs_data_t *settings, obs_source_t *source)
	{
		return new TsvReceiveSource(settings, source);
	}

	void obs_destroy(void *data)
	{
		if(data)
			delete reinterpret_cast<TsvReceiveSource *>(data);
	}

	uint32_t obs_get_width(void *data)
	{
		return reinterpret_cast<TsvReceiveSource *>(data)->GetWidth();
	}

	uint32_t obs_get_height(void *data)
	{
		return reinterpret_cast<TsvReceiveSource *>(data)->GetHeight();
	}

	void obs_get_defaults(void *data, obs_data_t *defaults)
	{
		return reinterpret_cast<TsvReceiveSource *>(data)->GetDefaults(defaults);
	}

	obs_properties_t *obs_get_properties(void *data)
	{
		return reinterpret_cast<TsvReceiveSource *>(data)->GetProperties();
	}

	void obs_update(void *data, obs_data_t *settings)
	{
		return reinterpret_cast<TsvReceiveSource *>(data)->UpdateProperties(settings);
	}

	void obs_video_tick(void *data, float seconds)
	{
		return reinterpret_cast<TsvReceiveSource *>(data)->OnTick(seconds);
	}

	void obs_video_render(void *data, gs_effect_t *effect)
	{
		return reinterpret_cast<TsvReceiveSource *>(data)->Render(effect);
	}
}

TsvReceiveSource::TsvReceiveSource(obs_data_t *settings, obs_source_t *source)
	: _source(source)
{
	this->UpdateProperties(settings);

	this->_tex_share_gl.init_with_server_launch();
}

TsvReceiveSource::~TsvReceiveSource()
{
	// Note: Enter graphics first to prevent race condition
	obs_enter_graphics();
	const auto lock = std::lock_guard(this->_access);

	if(this->_texture)
	{
		gs_texture_destroy(this->_texture);
		this->_texture = nullptr;
	}

	this->_source = nullptr;

	obs_leave_graphics();
}

uint32_t TsvReceiveSource::GetWidth()
{
	return this->_tex_width;
}

uint32_t TsvReceiveSource::GetHeight()
{
	return this->_tex_height;
}

obs_properties_t *TsvReceiveSource::GetProperties()
{
	obs_properties_t *properties = obs_properties_create();

	obs_properties_add_text(properties, PROPERTY_SHARED_TEXTURE_NAME.data(),
	                        obs_module_text(PROPERTY_SHARED_TEXTURE_NAME.data()), OBS_TEXT_DEFAULT);

	return properties;
}

void TsvReceiveSource::GetDefaults(obs_data_t *defaults)
{
	obs_data_set_default_string(defaults, PROPERTY_SHARED_TEXTURE_NAME.data(),
	                            obs_module_text(PROPERTY_SHARED_TEXTURE_NAME_DEFAULT.data()));
}

void TsvReceiveSource::UpdateProperties(obs_data_t *settings)
{
	// Check if sender name was updated
	const char *new_sender_name = obs_data_get_string(settings, PROPERTY_SHARED_TEXTURE_NAME.data());
	if(this->_shared_texture_name == new_sender_name)
		return;

	// If name was updated, reinitialize texture
	// Note: Enter graphics first to prevent race condition
	obs_enter_graphics();
	const auto lock = std::lock_guard(this->_access);

	if(this->_texture)
	{
		gs_texture_destroy(this->_texture);
		this->_texture = nullptr;
	}

	obs_leave_graphics();

	this->_shared_texture_name = obs_data_get_string(settings, PROPERTY_SHARED_TEXTURE_NAME.data());
}

void TsvReceiveSource::OnTick(float seconds)
{
	this->_elapsed_seconds += seconds;
	if(this->_elapsed_seconds >= SEARCH_INTERVAL)
	{
		this->_elapsed_seconds = 0;

		if(!this->_texture && !this->_shared_texture_name.empty())
			this->ImageSearchFunction();
	}
}

void TsvReceiveSource::Render(gs_effect_t *effect)
{
	// Note: Enter graphics before acuiring _lock to prevent race condition
	obs_enter_graphics();

	// Get default obs effect for drawing
	effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if(this->_texture)
	{
		const auto lock = std::lock_guard(this->_access);

		// Check whether the shared texture has changed
		if(this->_tex_share_gl.find_image(this->_shared_texture_name.c_str(), false) ==
		   ImageLookupResult::RequiresUpdate)
		{
			const auto data_lock = this->_tex_share_gl.find_image_data(this->_shared_texture_name.c_str(), true);

			const auto *data = data_lock.read();
			if(data != nullptr)
			{
				const uint32_t        tex_width  = data->width;
				const uint32_t        tex_height = data->height;
				const gs_color_format tex_format = GetSharedTextureFormat(data->format);

				this->UpdateTexture(tex_width, tex_height, tex_format);
			}
		}

		// Receive shared image
		GLuint *const gl_texture = reinterpret_cast<GLuint *>(gs_texture_get_obj(this->_texture));
		if(gl_texture)
		{
			const ImageExtent image_size{
				{0,						 0						 },
				{(GLsizei)this->_tex_width, (GLsizei)this->_tex_height},
			};

			GLint drawFboId = 0;
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);

			this->_tex_share_gl.recv_image(this->_shared_texture_name.c_str(), *gl_texture, GL_TEXTURE_2D, false,
			                               drawFboId, &image_size);
		}

		// Draw texture
		while(gs_effect_loop(effect, "Draw"))
		{
			obs_source_draw(this->_texture, 0, 0, 0, 0, false);
		}
	}

	obs_leave_graphics();
}

void TsvReceiveSource::ImageSearchFunction()
{
	obs_enter_graphics();
	const auto lock = std::lock_guard(this->_access);

	const auto  data_lock = this->_tex_share_gl.find_image_data(this->_shared_texture_name.c_str(), true);
	const auto *data      = data_lock.read();
	if(data != nullptr)
	{
		const uint32_t        tex_width  = data->width;
		const uint32_t        tex_height = data->height;
		const gs_color_format tex_format = GetSharedTextureFormat(data->format);

		this->UpdateTexture(tex_width, tex_height, tex_format);
	}

	obs_leave_graphics();
}

bool TsvReceiveSource::UpdateTexture(uint32_t width, uint32_t height, gs_color_format format)
{
	if(this->_texture)
		gs_texture_destroy(this->_texture);

	// Create texture
	this->_texture = gs_texture_create(width, height, format, 1, nullptr, 0);

	this->_tex_width  = width;
	this->_tex_height = height;
	this->_tex_format = format;

	return true;
}

gs_color_format TsvReceiveSource::GetSharedTextureFormat(ImgFormat format)
{
	switch(format)
	{
		case ImgFormat::R8G8B8A8:
			return GS_RGBA;
		case ImgFormat::B8G8R8A8:
			return GS_BGRA;
		default:
			return GS_UNKNOWN;
	}
}
