#pragma once

#include <obs-module.h>
#include <obs/graphics/graphics.h>
#include <texture_share_gl/texture_share_gl_client.hpp>

#include <mutex>
#include <string_view>

/*! \brief Texture sharing filter. Uses offscreen rendering to send source texture to other programs.
 */
class TsvReceiveSource
{
	public:
	static constexpr std::string_view PLUGIN_NAME = "texture-share-vk-source-plugin";

	TsvReceiveSource(obs_data_t *settings, obs_source_t *source);
	~TsvReceiveSource();

	/*! \brief Get source width
	 */
	uint32_t GetWidth();

	/*! \brief Get source height
	 */
	uint32_t GetHeight();

	/*! \brief Update settings
	 */
	void Update(obs_data_t *settings);

	/*! \brief Renders texture (in separate thread?). Waits for filter notification that more data is available
	 */
	// void OffscreenRender(uint32_t cx, uint32_t cy);

	/*! \brief Filter update. Only notifies offscreen renderer that new information is available
	 */
	void Render(gs_effect_t *effect);

	private:
	std::mutex _access;

	TextureShareGlClient _tex_share_gl;
	std::string          _shared_texture_name = "gd_img";

	obs_source_t *_source  = nullptr;
	gs_texture_t *_texture = nullptr;

	uint32_t        _tex_width  = 0;
	uint32_t        _tex_height = 0;
	gs_color_format _tex_format = GS_UNKNOWN;

	/*! \brief (Re-)initialize texture for copying
	 */
	bool UpdateTexture(uint32_t cx, uint32_t cy, gs_color_format format);

	static gs_color_format GetSharedTextureFormat(ImgFormat format);
};
