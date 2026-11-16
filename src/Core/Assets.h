#ifndef ASSETS_H
#define ASSETS_H

#include <Core/Types.h>
#include <Ream/RImage.h>
#include <AssetBroker/ABIconKey.h>
#include <AK/Nodes/AKImageFrame.h>
#include <filesystem>
#include <map>
#include <tuple>
#include <string>
#include <unordered_map>

namespace CZ { class ABCore; }
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
     * Through the cz-asset-broker service if it's running (shared with other apps as DMA-bufs),
     * otherwise loaded here.
     *
     * @return The image, or nullptr if the icon could not be found/decoded.
     */
    std::shared_ptr<RImage> loadIcon(const std::string &icon, Int32 size) noexcept;

    /**
     * @brief @p image (loaded by this class) again, at @p pixels (its larger side) for a screen with
     *        scale @p scale.
     *
     * Rendered again from the same file at that size (through the AssetBroker: shared with other
     * apps if the service is running). Returns @p image itself if it's not known.
     */
    std::shared_ptr<RImage> sized(const std::shared_ptr<RImage> &image, Int32 pixels, Int32 scale) noexcept;

    /**
     * @brief Shows @p image in @p frame at the size it's drawn at on each screen (scale specific,
     *        see AKImageFrame::setImageProvider()): never a 2x image scaled down on a 1x screen.
     */
    void show(AKImageFrame &frame, std::shared_ptr<RImage> image) noexcept;

    /// The provider show() uses for @p image (nullptr for nullptr).
    AKImageFrame::ImageProvider provider(std::shared_ptr<RImage> image) noexcept;

    std::shared_ptr<RImage> logo;          ///< System (Cuarzo) logo shown at the far left.
    std::shared_ptr<RImage> divider3Patch; ///< 3-patch line used by menu dividers.
    std::shared_ptr<RImage> check;         ///< Check mark used by toggles.
    std::shared_ptr<RImage> caret;         ///< Right arrow used by submenu items.

private:
    // Where each loaded image comes from (to load it at other sizes)
    struct Source
    {
        std::weak_ptr<RImage> image;
        ABIconKey key;              ///< Themed icon
        std::filesystem::path path; ///< Otherwise, the file
    };

    void remember(const std::shared_ptr<RImage> &image, Source source) noexcept;

    std::shared_ptr<ABCore> m_ab;
    std::unordered_map<const RImage*, Source> m_sources;
};

#endif // ASSETS_H
