#pragma once

#include <obs-module.h>
#include <obs/graphics/graphics.h>
#include <texture_share_vk/opengl/texture_share_gl_client.h>

#include <mutex>
#include <string_view>

/*! \brief Texture sharing filter. Uses offscreen rendering to send source texture to other programs.
 */
class OBSPluginTextureShare
{
	public:
	static constexpr std::string_view PLUGIN_NAME = "texture-share-filter-plugin";

	OBSPluginTextureShare(obs_data_t *settings, obs_source_t *source);
	~OBSPluginTextureShare();

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
	bool       _update_available = false;
	std::mutex _access;

	TextureShareGlClient _tex_share_gl;

	obs_source_t   *_source        = nullptr;
	gs_texrender_t *_render_target = nullptr;

	/*! \brief (Re-)initialize render target for offscreen rendering
	 */
	bool UpdateRenderTarget(uint32_t cx, uint32_t cy);
};
