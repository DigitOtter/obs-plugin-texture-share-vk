#pragma once

#include <obs-module.h>
#include <obs/graphics/graphics.h>
#include <texture_share_gl/texture_share_gl_client.hpp>

#include <future>
#include <mutex>
#include <string_view>

/*! \brief Texture sharing filter. Uses offscreen rendering to send source texture to other programs.
 */
class TsvReceiveSource
{
	public:
	static constexpr std::string_view PLUGIN_NAME                          = "texture-share-vk-source-plugin";
	static constexpr std::string_view PROPERTY_SHARED_TEXTURE_NAME         = "shared_texture_name";
	static constexpr std::string_view PROPERTY_SHARED_TEXTURE_NAME_DEFAULT = "obs_shared";

	// Time (in seconds) between searches for shared textures
	static constexpr float SEARCH_INTERVAL = 1.0;

	TsvReceiveSource(obs_data_t *settings, obs_source_t *source);
	~TsvReceiveSource();

	/*! \brief Get source width
	 */
	uint32_t GetWidth();

	/*! \brief Get source height
	 */
	uint32_t GetHeight();

	obs_properties_t *GetProperties();

	/*! \brief Default settings
	 */
	void GetDefaults(obs_data_t *defaults);

	/*! \brief Update settings
	 */
	void UpdateProperties(obs_data_t *settings);

	/*! \brief Renders texture (in separate thread?). Waits for filter notification that more data is available
	 */
	// void OffscreenRender(uint32_t cx, uint32_t cy);

	/*! \brief Called every frame with the amount of elapsed settings. Regulates how often _tex_share_gl looks for
	 * images
	 */
	void OnTick(float seconds);

	/*! \brief Filter update. Only notifies offscreen renderer that new information is available
	 */
	void Render(gs_effect_t *effect);

	private:
	std::mutex _access;

	TextureShareGlClient _tex_share_gl;
	std::string          _shared_texture_name;
	float                _elapsed_seconds = 0;

	obs_source_t *_source  = nullptr;
	gs_texture_t *_texture = nullptr;

	uint32_t        _tex_width  = 0;
	uint32_t        _tex_height = 0;
	gs_color_format _tex_format = GS_UNKNOWN;

	void ImageSearchFunction();

	/*! \brief (Re-)initialize texture for copying
	 */
	bool UpdateTexture(uint32_t cx, uint32_t cy, gs_color_format format);

	static gs_color_format GetSharedTextureFormat(ImgFormat format);
};
