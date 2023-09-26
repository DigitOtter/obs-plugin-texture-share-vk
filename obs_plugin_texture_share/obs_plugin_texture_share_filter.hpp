#pragma once

#include <obs-module.h>
#include <obs/graphics/graphics.h>
#include <texture_share_vk/opengl/texture_share_gl_client.h>

#include <mutex>
#include <string_view>

/*! \brief Texture sharing filter. Uses offscreen rendering to send source texture to other programs.
 */
class OBSPluginTextureShareFilter
{
	public:
	static constexpr std::string_view PLUGIN_NAME = "texture-share-filter-plugin";

	OBSPluginTextureShareFilter(obs_data_t *settings, obs_source_t *source);
	~OBSPluginTextureShareFilter();

	/*! \brief Update settings
	 */
	void Update(obs_data_t *settings);

	/*! \brief Renders texture (in separate thread?). Waits for filter notification that more data is available
	 */
	void OffscreenRender(uint32_t cx, uint32_t cy);

	/*! \brief Filter update. Only notifies offscreen renderer that new information is available
	 */
	void Render(gs_effect_t *effect);

	private:
	enum RENDER_STATE
	{
		WAITING,
		UPDATE_AVAILABLE,
		OFFSCREEN_RENDERING
	};
	RENDER_STATE _render_state = WAITING;
	bool       _update_available = false;
	std::mutex _access;

	TextureShareGlClient _tex_share_gl;
	std::string          _shared_texture_name = "gd_img";

	obs_source_t   *_source        = nullptr;
	gs_texrender_t *_render_target = nullptr;

	uint32_t _tex_width  = 0;
	uint32_t _tex_height = 0;

	/*! \brief (Re-)initialize render target for offscreen rendering
	 */
	bool UpdateRenderTarget(uint32_t cx, uint32_t cy);
};
