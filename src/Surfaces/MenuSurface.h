#ifndef MENUSURFACE_H
#define MENUSURFACE_H

#include <Marco/Roles/MMenu.h>
#include <AK/Nodes/AKRoundSolidColor.h>
#include <AK/AKTheme.h>
#include <Core/Types.h>
#include <functional>
#include <memory>

using namespace CZ;

/**
 * @brief A dropdown popup that shows one menu's items and can open one submenu at a time.
 *
 * A MenuSurface does not own its content: when a menu is shown, the owner reparents that menu's
 * MenuItemsContainer into the surface (see host()). This is the same relationship Firmament has with
 * the topbar menus.
 *
 * Just like Firmament manages the topbar-level menus, a MenuSurface owns two child MenuSurfaces and
 * shows one submenu at a time, alternating between them so that moving the pointer between items
 * swaps the visible submenu smoothly: the outgoing one is only unmapped once the incoming one is
 * about to become visible (via onWillMap / beforeMap).
 */
class MenuSurface final : public MMenu
{
public:
    MenuSurface() noexcept;

    /// Reparents @p menu's content into this surface (detaching whatever it showed before). No-op if
    /// it already hosts that menu. Passing nullptr just clears the current content.
    void host(MenuModel *menu) noexcept;
    MenuItemsContainer *hostedContent() const noexcept { return m_hostedContent.get(); }

    AKRoundSolidColor menuItemOutline { AKTheme::SystemBlue.light(), CZBorderRadius::Make(4) };

    /// Selects (hovers) an item; opens/closes its submenu if it is a SubMenu.
    void setActiveMenuItem(MenuItem *item = nullptr) noexcept;
    CZWeak<MenuItem> activeMenuItem;

    /// Dims the selection outline while an item is being pressed, restoring it on release.
    void setOutlinePressed(bool pressed) noexcept;

    /// Opens this popup under a topbar item (parent = topbar surface).
    void openUnder(MSurface *parent, AKNode *anchorItem) noexcept;
    /// Opens this popup to the right of a submenu item (parent = the parent popup).
    void openToRight(MSurface *parent, AKNode *anchorItem) noexcept;

    /// Ran once, inside onWillMap, right before the popup becomes visible.
    std::function<void()> beforeMap;

private:
    void openSubMenu(SubMenu *item) noexcept;
    void closeSubMenu() noexcept;
    void refreshSize() noexcept;

    CZWeak<MenuItemsContainer> m_hostedContent; ///< The menu content currently reparented into us.

    // The two pooled submenu surfaces (lazy), and which is the open one / the one being replaced.
    std::unique_ptr<MenuSurface> m_subA, m_subB;
    CZWeak<MenuSurface> subMenu, prevSubMenu;
};

#endif // MENUSURFACE_H
