#ifndef ASSETS_H
#define ASSETS_H

#include <Core/Types.h>
#include <Ream/RImage.h>
#include <Ream/DRM/RDRMFormat.h>
#include <AK/Nodes/AKImageFrame.h>
#include <filesystem>
#include <map>
#include <tuple>
#include <string>
#include <unordered_map>

namespace CZ { class XDGIcon; }
using namespace CZ;

/**
 * @brief Loads and holds the images/icons shared across Firmament nodes.
 *
 * Owned by Firmament and reachable through GetFirm()->assets.
 */
class Assets
{
public:
    Assets() noexcept = default;

    /// Loads every asset. Call once, after XDGKit and RCore are available.
    void init() noexcept;

    /**
     * @brief Loads a themed icon by freedesktop name at @p size logical px (rendered at 2x).
     *
     * @return The image, or nullptr if the icon could not be found/decoded.
     */
    std::shared_ptr<RImage> loadIcon(const std::string &icon, Int32 size) noexcept;

    /**
     * @brief @p image (loaded by this class) again, at @p pixels (its larger side) for a screen with
     *        scale @p scale.
     *
     * SVGs are rendered again at that size. Raster themed icons are looked up again for that size
     * and scale (scaled, antialiased, only if the theme has no such size). Other files
     * are loaded again at that size. Cached. Returns @p image itself if it's not known.
     */
    std::shared_ptr<RImage> sized(const std::shared_ptr<RImage> &image, Int32 pixels, Int32 scale) noexcept;

    /**
     * @brief Shows @p image in @p frame at the size it's drawn at on each screen (scale specific,
     *        see AKImageFrame::setImageProvider()): never a 2x image scaled down on a 1x screen.
     */
    void show(AKImageFrame &frame, std::shared_ptr<RImage> image) noexcept;

    /// The provider show() uses for @p image (nullptr for nullptr).
    AKImageFrame::ImageProvider provider(std::shared_ptr<RImage> image) noexcept;

    // A texture format guaranteed to be sampleable on the main device (ABGR8888).
    RDRMFormat safeFormat { 0, {} };

    std::shared_ptr<RImage> logo;          ///< System (Cuarzo) logo shown at the far left.
    std::shared_ptr<RImage> divider3Patch; ///< 3-patch line used by menu dividers.
    std::shared_ptr<RImage> check;         ///< Check mark used by toggles.
    std::shared_ptr<RImage> caret;         ///< Right arrow used by submenu items.

private:
    std::shared_ptr<RImage> loadFile(const std::filesystem::path &path, SkISize size, const std::string &iconName = {}) noexcept;
    const XDGIcon *findIcon(const std::string &name, Int32 size, Int32 scale) noexcept;

    // Where each loaded image comes from (to load it at other sizes).
    struct Source
    {
        std::weak_ptr<RImage> image;
        std::string iconName;       ///< Themed icon (looked up again per size and scale)
        std::filesystem::path path; ///< Otherwise, the file
    };
    std::unordered_map<const RImage*, Source> m_sources;
    std::map<std::tuple<std::string, Int32, Int32>, std::shared_ptr<RImage>> m_sized; // By source, size and scale
};

#endif // ASSETS_H
