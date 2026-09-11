#ifndef ASSETS_H
#define ASSETS_H

#include <Core/Types.h>
#include <Ream/RImage.h>
#include <Ream/DRM/RDRMFormat.h>
#include <string>

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

    // A texture format guaranteed to be sampleable on the main device (ABGR8888).
    RDRMFormat safeFormat { 0, {} };

    std::shared_ptr<RImage> logo;          ///< System (Cuarzo) logo shown at the far left.
    std::shared_ptr<RImage> divider3Patch; ///< 3-patch line used by menu dividers.
    std::shared_ptr<RImage> check;         ///< Check mark used by toggles.
    std::shared_ptr<RImage> caret;         ///< Right arrow used by submenu items.
};

#endif // ASSETS_H
