#ifndef MENUMODEL_H
#define MENUMODEL_H

#include <Core/Types.h>
#include <Heaven/Bar/HNObject.h>
#include <string>
#include <unordered_map>
#include <memory>

using namespace CZ;
using namespace CZ::HNBarAPI;

/**
 * @brief App-side model of a menu.
 *
 * Backed by a Heaven HNMenu, or standalone (nullptr) for the built-in "Desk" and system menus.
 *
 * A menu shows up in two possible ways depending on where it lives:
 *  - as a topbar item (child of a topbar): a TopbarItem per screen.
 *  - as a submenu item (child of another menu): a single SubMenu node.
 * The model does NOT own a MenuSurface: it only owns its MenuItemsContainer (the item rows). When it
 * is activated, the opener (Firmament for topbar menus, a MenuSurface for submenus) reparents that
 * container into one of its two pooled MenuSurfaces. The topbar/submenu nodes point back to the model.
 */
class MenuModel
{
public:
    explicit MenuModel(HNMenu *hnMenu = nullptr) noexcept;
    ~MenuModel();

    static MenuModel *From(HNObject *o) noexcept { return o ? static_cast<MenuModel*>(o->userData) : nullptr; }

    HNMenu *hnMenu() const noexcept { return m_hnMenu; }

    void setTitle(const std::string &title) noexcept;
    const std::string &title() const noexcept { return m_title; }
    void setIconName(const std::string &iconName) noexcept;
    void setEnabled(bool enabled) noexcept;

    MenuItemsContainer *content() noexcept;      ///< The item rows (lazy-created); reparented into a MenuSurface when shown.

    TopbarItem *topbarItem(TopbarSurface *surface) noexcept;  ///< Per-screen topbar node (lazy).
    void removeTopbarItem(TopbarSurface *surface) noexcept;
    void detachTopbarItems() noexcept;

    SubMenu *subMenuItem() noexcept;             ///< Single submenu node (lazy).
    void detachSubMenuItem() noexcept;

    void detach() noexcept;                       ///< Detach every representation from its parent.

private:
    HNMenu *m_hnMenu;
    std::string m_title;
    std::string m_iconName;
    bool m_enabled { true };

    std::unique_ptr<MenuItemsContainer> m_content;
    std::unordered_map<TopbarSurface*, std::unique_ptr<TopbarItem>> m_topbarItems;
    std::unique_ptr<SubMenu> m_subMenuItem;
};

#endif // MENUMODEL_H
