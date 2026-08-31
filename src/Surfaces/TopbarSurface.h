#ifndef TOPBARSURFACE_H
#define TOPBARSURFACE_H

#include <Nodes/TopbarItem.h>
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
 * Layout: a fixed application-title text on the left, and to its right a menu area that holds the
 * active client's topbar row (swapped in/out on active-client change). An absolute, rounded outline
 * node slides under the currently selected menu title.
 */
class TopbarSurface final : public MLayerSurface
{
public:
    TopbarSurface(MScreen &screen) noexcept;

    void setActiveItem(TopbarItem *item = nullptr) noexcept;

    MVibrancyView vibrancy { this };
    TopbarItem logo { &vibrancy };
    TopbarItem appTitle { &vibrancy };

    AKContainer menusArea { YGFlexDirectionRow, false, &vibrancy };
    TopbarItem m1 { &menusArea };
    TopbarItem m2 { &menusArea };
    TopbarItem m3 { &menusArea };
    TopbarItem m4 { &menusArea };
    TopbarItem m5 { &menusArea };
    TopbarItem m6 { &menusArea };

    AKRoundSolidColor outline { SkColorSetARGB(32, 0, 0, 0), CZBorderRadius::Make(4), &vibrancy };

    void pointerButtonEvent(const CZPointerButtonEvent &e) override;

private:
    TopbarModel *m_activeTopbar {};
};

#endif // TOPBARSURFACE_H
