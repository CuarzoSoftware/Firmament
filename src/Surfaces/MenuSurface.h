#ifndef MENUSURFACE_H
#define MENUSURFACE_H

#include <Marco/Roles/MMenu.h>
#include <AK/Nodes/AKRoundSolidColor.h>
#include <AK/AKTheme.h>
#include <Core/Types.h>
#include <functional>
#include <memory>
#include <vector>

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
 *
 * Keyboard (a topbar menu grabs it, its submenus get the keys through it): Up/Down select the
 * previous/next item, Right (or Enter) opens a submenu and selects its first item, Left (or Escape)
 * closes it, Enter/Space activate the selected item, and Left/Right on a menu with no submenu to
 * open/close switch to the previous/next topbar menu. Escape closes the menus.
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

    /**
     * @brief Activates a clicked item like macOS: its highlight blinks a few times, then its
     *        onClicked runs and the whole menu chain is dismissed.
     *
     * Hovering other items does nothing meanwhile. Safe if the item or menu goes away meanwhile.
     */
    static void Activate(MenuItem &item) noexcept;

    /// Whether an item of this menu is blinking before being activated.
    bool activating() const noexcept { return m_activating; }

    /// Dims the selection outline while an item is being pressed, restoring it on release.
    void setOutlinePressed(bool pressed) noexcept;

    /// Opens this popup under a topbar item (parent = topbar surface), grabbing the keyboard with
    /// @p grab (the event that opened it). @p selectFirst: opened with the keyboard.
    void openUnder(MSurface *parent, AKNode *anchorItem, const CZInputEvent *grab = nullptr, bool selectFirst = false) noexcept;
    /// Opens this popup to the right of a submenu item (parent = the parent popup).
    void openToRight(MSurface *parent, AKNode *anchorItem) noexcept;

    /// Ran once, inside onWillMap, right before the popup becomes visible.
    std::function<void()> beforeMap;

protected:
    void keyboardKeyEvent(const CZKeyboardKeyEvent &e) override;

private:
    /// Its items that can be selected (not dividers, hidden or disabled ones), in order.
    std::vector<MenuItem*> selectableItems() const noexcept;

    /// Selects the first item (opening it at its submenu if it has one).
    void selectFirst() noexcept;

    bool m_activating { false };
    void openSubMenu(SubMenu *item) noexcept;
    void closeSubMenu() noexcept;
    void refreshSize() noexcept;

    /// Sizes the popup and its content for the UI scale of the screen it opens on.
    void setUIScale(float scale) noexcept;
    float m_uiScale { 1.f };

    CZWeak<MenuItemsContainer> m_hostedContent; ///< The menu content currently reparented into us.

    // The two pooled submenu surfaces (lazy), and which is the open one / the one being replaced.
    std::unique_ptr<MenuSurface> m_subA, m_subB;
    CZWeak<MenuSurface> subMenu, prevSubMenu;
};

#endif // MENUSURFACE_H
