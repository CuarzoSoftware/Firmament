#include <Models/TopbarModel.h>
#include <Models/MenuModel.h>
#include <Heaven/Bar/HNTopbar.h>
#include <algorithm>

TopbarModel::TopbarModel(HNTopbar *hnTopbar) noexcept : m_hnTopbar(hnTopbar)
{
    if (hnTopbar)
        hnTopbar->userData = this;
}

TopbarModel::~TopbarModel()
{
    if (m_hnTopbar && m_hnTopbar->userData == this)
        m_hnTopbar->userData = nullptr;
}

void TopbarModel::insertMenu(MenuModel *menu, MenuModel *before) noexcept
{
    if (!menu)
        return;

    m_menus.erase(std::remove(m_menus.begin(), m_menus.end(), menu), m_menus.end());

    auto pos { before ? std::find(m_menus.begin(), m_menus.end(), before) : m_menus.end() };
    m_menus.insert(pos, menu);
}

void TopbarModel::removeMenu(MenuModel *menu) noexcept
{
    m_menus.erase(std::remove(m_menus.begin(), m_menus.end(), menu), m_menus.end());
}
