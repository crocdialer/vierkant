#pragma once

#include "vierkant/Rasterizer.hpp"

namespace vierkant
{

/**
 * @brief   DisplayOutput turns scene-linear images into display-values.
 *          It applies exposure, tone-mapping and the output-encoding (SDR or HDR10),
 *          and composites an optional SDR ui-image on top.
 */
class DisplayOutput
{
public:
    //! output-encoding
    enum class Encoding : uint32_t
    {
        //! gamma-encoded, BT.709
        SDR = 0,

        //! ST 2084 (PQ) encoded, BT.2020, for VK_COLOR_SPACE_HDR10_ST2084_EXT
        HDR10 = 1
    };

    struct settings_t
    {
        //! exposure setting for tone-mapping
        float exposure = 1.f;

        //! gamma for SDR-encoding, also used to linearize the ui-image
        float gamma = 2.2f;

        //! peak display-luminance in cd/m², used for HDR output
        float peak_nits = 1000.f;

        //! luminance of SDR reference-white in cd/m², used for HDR output
        float paper_white_nits = 203.f;
    };

    //! display-settings
    settings_t settings;

    DisplayOutput() = default;

    explicit DisplayOutput(const vierkant::DevicePtr &device);

    /**
     * @brief   stage a fullscreen display-pass into a provided Rasterizer.
     *
     * @param   renderer    a provided vierkant::Rasterizer.
     * @param   scene       a scene-linear image.
     * @param   ui          optional ui-image, sRGB-encoded with premultiplied alpha.
     * @param   encoding    the output-encoding.
     */
    void draw(vierkant::Rasterizer &renderer, const vierkant::ImagePtr &scene, const vierkant::ImagePtr &ui = nullptr,
              Encoding encoding = Encoding::SDR) const;

    inline explicit operator bool() const { return static_cast<bool>(m_empty_ui); };

private:
    vierkant::drawable_t m_drawable;

    //! transparent 1x1 image, used when no ui-image is provided
    vierkant::ImagePtr m_empty_ui;
};

}// namespace vierkant
