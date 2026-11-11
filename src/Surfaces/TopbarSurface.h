#ifndef TOPBARSURFACE_H
#define TOPBARSURFACE_H

#include <Nodes/TopbarItem.h>
#include <Nodes/StatusIcon.h>
#include <Marco/Roles/MLayerSurface.h>
#include <Marco/Nodes/MVibrancyView.h>
#include <AK/Nodes/AKRoundSolidColor.h>
#include <AK/Nodes/AKContainer.h>
#include <AK/Nodes/AKText.h>
#include <Core/Types.h>

using namespace CZ;

/**
 * @brief The global menu bar, one per screen.
 *
 * Layout: the system logo and the active app title on the left, and the active menu bar's items in
 * menusArea (filled by Firmament from the active source — a client's topbar or the Desk defaults).
 * An absolute rounded outline slides under the currently selected topbar item.
 */
class TopbarSurface final : public MLayerSurface
{
public:
    TopbarSurface(MScreen &screen) noexcept;

    /// Places the selection outline under @p item (a menu's item, or the indicators), nullptr hides it.
    void setActiveItem(AKNode *item = nullptr) noexcept;

    /// The bar's UI scale (see Firmament::uiScale()).
    float uiScale() const noexcept { return m_uiScale; }

    /// Resizes the bar and its items for Firmament::uiScale().
    void updateUIScale() noexcept;

    /// Shows Firmament's current Status in the indicators.
    void updateStatus() noexcept;

    MVibrancyView vibrancy { this };
    TopbarItem logo { &vibrancy };     ///< Opens the system menu.
    TopbarItem appTitle { &vibrancy };  ///< Active application name (bold, non-interactive).
    AKContainer menusArea { YGFlexDirectionRow, false, &vibrancy };
    AKRoundSolidColor outline { SkColorSetARGB(96, 0, 0, 0), CZBorderRadius::Make(4), &vibrancy };
    AKContainer spacer { YGFlexDirectionRow, false, &vibrancy };
    AKContainer indicators { YGFlexDirectionRow, false, &vibrancy }; ///< Status icons, before the clock
    StatusIcon network { &indicators }; ///< Wi-Fi signal / wired / offline (see Status)
    StatusIcon volume { &indicators };  ///< Output volume
    StatusIcon battery { &indicators }; ///< Battery level and charging state
    AKText clock { "", &vibrancy };    ///< Date and time (see Firmament::clockText())
    TopbarItem sizeItem { &vibrancy }; ///< Bar size menu (gear icon), right corner

    void pointerButtonEvent(const CZPointerButtonEvent &e) override;

private:
    float m_uiScale { 0.f };
};

#endif // TOPBARSURFACE_H
