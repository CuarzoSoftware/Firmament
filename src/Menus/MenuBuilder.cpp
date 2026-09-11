#include <Menus/MenuBuilder.h>
#include <Models/MenuModel.h>
#include <Nodes/MenuItemsContainer.h>
#include <Nodes/Action.h>
#include <Nodes/Divider.h>
#include <Nodes/Toggle.h>
#include <Nodes/SubMenu.h>

MenuBuilder::~MenuBuilder() noexcept {}

MenuModel *MenuBuilder::newMenu(const std::string &title) noexcept
{
    auto menu { std::make_unique<MenuModel>() };
    menu->setTitle(title);
    MenuModel *raw { menu.get() };
    m_menus.push_back(std::move(menu));
    return raw;
}

Action *MenuBuilder::addAction(MenuModel *menu, const std::string &text,
                               const std::string &shortcut, const std::string &icon) noexcept
{
    auto node { std::make_unique<Action>(text, menu->content()) };
    if (!shortcut.empty()) node->setShortcut(shortcut);
    if (!icon.empty())     node->setIconName(icon);
    Action *raw { node.get() };
    m_items.push_back(std::move(node));
    return raw;
}

Divider *MenuBuilder::addDivider(MenuModel *menu, const std::string &title) noexcept
{
    auto node { std::make_unique<Divider>(title, menu->content()) };
    Divider *raw { node.get() };
    m_items.push_back(std::move(node));
    return raw;
}

Toggle *MenuBuilder::addToggle(MenuModel *menu, const std::string &text, bool checked) noexcept
{
    auto node { std::make_unique<Toggle>(text, menu->content()) };
    node->setChecked(checked);
    Toggle *raw { node.get() };
    m_items.push_back(std::move(node));
    return raw;
}

MenuModel *MenuBuilder::addSubMenu(MenuModel *parent, const std::string &title,
                                   const std::string &icon) noexcept
{
    MenuModel *child { newMenu(title) };
    child->setIconName(icon);
    child->subMenuItem()->setParent(parent->content()); // adds the SubMenu row into the parent
    return child;
}
