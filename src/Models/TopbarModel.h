#ifndef TOPBARMODEL_H
#define TOPBARMODEL_H

#include <Core/Types.h>
#include <Heaven/Bar/HNObject.h>
#include <vector>

using namespace CZ;
using namespace CZ::HNBarAPI;

/**
 * @brief App-side model of a menu bar (an ordered set of top-level menus).
 *
 * Backed by a Heaven HNTopbar, or standalone (nullptr) for the built-in "Desk" bar. It only holds
 * the ordered list of its menus; Firmament lays their per-screen TopbarItems into each screen's
 * TopbarSurface when this bar is the active one.
 */
class TopbarModel
{
public:
    explicit TopbarModel(HNTopbar *hnTopbar = nullptr) noexcept;
    ~TopbarModel();

    static TopbarModel *From(HNObject *o) noexcept { return o ? static_cast<TopbarModel*>(o->userData) : nullptr; }

    HNTopbar *hnTopbar() const noexcept { return m_hnTopbar; }

    const std::vector<MenuModel*> &menus() const noexcept { return m_menus; }

    /// Inserts @p menu before @p before (appends if @p before is null).
    void insertMenu(MenuModel *menu, MenuModel *before) noexcept;
    void removeMenu(MenuModel *menu) noexcept;

private:
    HNTopbar *m_hnTopbar;
    std::vector<MenuModel*> m_menus;
};

#endif // TOPBARMODEL_H
