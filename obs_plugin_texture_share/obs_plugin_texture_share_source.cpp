#include "obs_plugin_texture_share/obs_plugin_texture_share_source.hpp"

#include <obs.h>
#include <obs/graphics/graphics.h>
#include <util/c99defs.h>


extern "C"
{
    /* Required OBS module commands (required) */
    OBS_DECLARE_MODULE();
	OBS_MODULE_USE_DEFAULT_LOCALE(OBSPluginTextureShareSource::PLUGIN_NAME.data(), "en-US");

	// Define plugin info
	const char *obs_get_name(void *type_data);
	void       *obs_create(obs_data_t *settings, obs_source_t *source);
	void        obs_destroy(void *data);
	uint32_t    obs_get_width(void *data);
	uint32_t    obs_get_height(void *data);
	void        obs_update(void *data, obs_data_t *settings);
	void        obs_video_render(void *data, gs_effect_t *effect);

	constexpr struct obs_source_info obs_plugin_texture_share_info = {
		.id           = OBSPluginTextureShareSource::PLUGIN_NAME.data(),
		.type         = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
		.get_name     = obs_get_name,
		.create       = obs_create,
		.destroy      = obs_destroy,
		.get_width    = obs_get_width,
		.get_height   = obs_get_height,
		.update       = obs_update,
		.video_render = obs_video_render,
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
		return OBSPluginTextureShareSource::PLUGIN_NAME.data();
	}

	void *obs_create(obs_data_t *settings, obs_source_t *source)
	{
		return new OBSPluginTextureShareSource(settings, source);
	}

	void obs_destroy(void *data)
	{
		if(data)
			delete reinterpret_cast<OBSPluginTextureShareSource *>(data);
	}

	uint32_t obs_get_width(void *data)
	{
		return reinterpret_cast<OBSPluginTextureShareSource *>(data)->GetWidth();
	}

	uint32_t obs_get_height(void *data)
	{
		return reinterpret_cast<OBSPluginTextureShareSource *>(data)->GetHeight();
	}

	void obs_update(void *data, obs_data_t *settings)
	{
		return reinterpret_cast<OBSPluginTextureShareSource *>(data)->Update(settings);
	}

	void obs_video_render(void *data, gs_effect_t *effect)
	{
		return reinterpret_cast<OBSPluginTextureShareSource *>(data)->Render(effect);
	}
}

OBSPluginTextureShareSource::OBSPluginTextureShareSource(obs_data_t *settings, obs_source_t *source)
	: _source(obs_source_get_ref(source))
{
	this->_tex_share_gl.init_with_server_launch();
}

OBSPluginTextureShareSource::~OBSPluginTextureShareSource()
{
	const auto lock = std::lock_guard(this->_access);

	if(this->_texture)
	{
		obs_enter_graphics();
		gs_texture_destroy(this->_texture);
		obs_leave_graphics();
		this->_texture = nullptr;
	}

	if(this->_source)
	{
		obs_source_release(this->_source);
		this->_source = nullptr;
	}
}

uint32_t OBSPluginTextureShareSource::GetWidth()
{
	return this->_tex_width;
}

uint32_t OBSPluginTextureShareSource::GetHeight()
{
	return this->_tex_height;
}

void OBSPluginTextureShareSource::Render(gs_effect_t *effect)
{
	const auto lock = std::lock_guard(this->_access);

	// Get default obs effect for drawing
	effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	obs_enter_graphics();

	if(!this->_texture)
	{
		bool force_update = false;

		ImageLookupResult res = this->_tex_share_gl.find_image(this->_shared_texture_name.c_str(), false);
		if(res == ImageLookupResult::RequiresUpdate)
			force_update = true;

		if(res != ImageLookupResult::Error && res != ImageLookupResult::NotFound)
		{
			const auto data_lock =
				this->_tex_share_gl.find_image_data(this->_shared_texture_name.c_str(), force_update);

			const auto *data = data_lock.read();
			if(data != nullptr)
			{
				const uint32_t        tex_width  = data->width;
				const uint32_t        tex_height = data->height;
				const gs_color_format tex_format = GetSharedTextureFormat(data->format);

				this->UpdateTexture(tex_width, tex_height, tex_format);
			}
		}
	}
	else
	{
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
	}

	// Receive texture
	if(this->_texture)
	{
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
	}

	obs_leave_graphics();

	// Draw texture
	while(gs_effect_loop(effect, "Draw"))
	{
		obs_source_draw(this->_texture, 0, 0, 0, 0, false);
	}
}

bool OBSPluginTextureShareSource::UpdateTexture(uint32_t width, uint32_t height, gs_color_format format)
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

gs_color_format OBSPluginTextureShareSource::GetSharedTextureFormat(ImgFormat format)
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
