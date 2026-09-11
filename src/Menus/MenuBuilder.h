#ifndef MENUBUILDER_H
#define MENUBUILDER_H

#include <Core/Types.h>
#include <string>
#include <vector>
#include <memory>

/**
 * @brief Helper base to build standalone (non-Heaven) menus for the built-in "Desk" and system UI.
 *
 * Owns every MenuModel and item node it creates, so subclasses just describe the menu tree in their
 * constructor. These menus behave exactly like Heaven-driven ones, they simply aren't backed by any
 * HNObject.
 */
class MenuBuilder
{
public:
    virtual ~MenuBuilder() noexcept;

protected:
    /// Creates a standalone menu (owned here). Not attached to anything by itself.
    MenuModel *newMenu(const std::string &title = "") noexcept;

    Action *addAction(MenuModel *menu, const std::string &text,
                      const std::string &shortcut = "", const std::string &icon = "") noexcept;
    Divider *addDivider(MenuModel *menu, const std::string &title = "") noexcept;
    Toggle *addToggle(MenuModel *menu, const std::string &text, bool checked = false) noexcept;

    /// Creates a submenu of @p parent (its SubMenu row is added to @p parent's content) and returns
    /// the child menu so more items can be added to it.
    MenuModel *addSubMenu(MenuModel *parent, const std::string &title,
                          const std::string &icon = "") noexcept;

    std::vector<std::unique_ptr<MenuModel>> m_menus;
    std::vector<std::unique_ptr<MenuItem>> m_items;
};

#endif // MENUBUILDER_H
