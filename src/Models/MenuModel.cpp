#include <Models/MenuModel.h>
#include <Nodes/MenuItemsContainer.h>
#include <Nodes/TopbarItem.h>
#include <Nodes/SubMenu.h>
#include <Heaven/Bar/HNMenu.h>

MenuModel::MenuModel(HNMenu *hnMenu) noexcept : m_hnMenu(hnMenu)
{
    if (hnMenu)
    {
        hnMenu->userData = this;
        m_title = hnMenu->title();
        m_iconName = hnMenu->icon();
        m_enabled = hnMenu->enabled();
    }
}

MenuModel::~MenuModel()
{
    if (m_hnMenu && m_hnMenu->userData == this)
        m_hnMenu->userData = nullptr;

    // The child item nodes are owned elsewhere (Heaven objects / Desk builders), so detach them
    // before our content container goes away.
    if (m_content)
        while (!m_content->children().empty())
            m_content->children().back()->setParent(nullptr);
}

MenuItemsContainer *MenuModel::content() noexcept
{
    if (!m_content)
        m_content = std::make_unique<MenuItemsContainer>();
    return m_content.get();
}

TopbarItem *MenuModel::topbarItem(TopbarSurface *surface) noexcept
{
    auto it { m_topbarItems.find(surface) };
    if (it != m_topbarItems.end())
        return it->second.get();

    auto item { std::make_unique<TopbarItem>() };
    item->setText(m_title);
    item->menu = this;
    TopbarItem *raw { item.get() };
    m_topbarItems[surface] = std::move(item);
    return raw;
}

void MenuModel::removeTopbarItem(TopbarSurface *surface) noexcept
{
    m_topbarItems.erase(surface);
}

void MenuModel::detachTopbarItems() noexcept
{
    for (auto &[surface, item] : m_topbarItems)
        item->setParent(nullptr);
}

SubMenu *MenuModel::subMenuItem() noexcept
{
    if (!m_subMenuItem)
    {
        m_subMenuItem = std::make_unique<SubMenu>(m_title);
        m_subMenuItem->setIconName(m_iconName);
        m_subMenuItem->setEnabled(m_enabled);
        m_subMenuItem->submenu = this;
    }
    return m_subMenuItem.get();
}

void MenuModel::detachSubMenuItem() noexcept
{
    if (m_subMenuItem)
        m_subMenuItem->setParent(nullptr);
}

void MenuModel::detach() noexcept
{
    detachTopbarItems();
    detachSubMenuItem();
}

void MenuModel::setTitle(const std::string &title) noexcept
{
    m_title = title;

    for (auto &[surface, item] : m_topbarItems)
        item->setText(title);

    if (m_subMenuItem)
        m_subMenuItem->setText(title);
}

void MenuModel::setIconName(const std::string &iconName) noexcept
{
    m_iconName = iconName;

    if (m_subMenuItem)
        m_subMenuItem->setIconName(iconName);
}

void MenuModel::setEnabled(bool enabled) noexcept
{
    m_enabled = enabled;

    if (m_subMenuItem)
        m_subMenuItem->setEnabled(enabled);
}
