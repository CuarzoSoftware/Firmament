#include <Surfaces/MenuSurface.h>
#include <Core/CZWeak.h>
#include <Core/CZTimer.h>
#include <cmath>
#include <Surfaces/TopbarSurface.h>
#include <Models/MenuModel.h>
#include <Nodes/MenuItemsContainer.h>
#include <Nodes/MenuItem.h>
#include <Nodes/SubMenu.h>
#include <Core/Firmament.h>
#include <cassert>

MenuSurface::MenuSurface() noexcept
{
    m_uiScale = 0.f;
    setUIScale(1.f);

    menuItemOutline.layout().setPositionType(YGPositionTypeAbsolute);
    menuItemOutline.layout().setWidthPercent(100.f);

    onWillMap.subscribe(this, [this]{
        if (!beforeMap)
            return;
        auto cb { beforeMap };
        beforeMap = nullptr;
        cb();
    });

    // When this popup hides, drop its selection and close its open submenu. If it was the active
    // topbar menu (and nothing replaced it), Firmament hides the topbar item's selection outline.
    onMappedChanged.subscribe(this, [this]{
        if (!mapped())
        {
            setActiveMenuItem(nullptr);
            GetFirm()->onMenuUnmapped(this);
        }
    });
}

void MenuSurface::host(MenuModel *menu) noexcept
{
    MenuItemsContainer *c { menu ? menu->content() : nullptr };

    if (m_hostedContent.get() == c)
        return;

    // Drop the previous menu's selection outline before swapping its content out.
    setActiveMenuItem(nullptr);

    if (m_hostedContent)
        m_hostedContent->setParent(nullptr);

    m_hostedContent.reset(c);

    if (c)
        c->setParent(this);
}

void MenuSurface::setActiveMenuItem(MenuItem *item) noexcept
{
    // The item being activated keeps the selection while it blinks.
    if (item == activeMenuItem || m_activating)
        return;
    if (item) assert(item->parentWindow() == this);

    if (activeMenuItem)
        activeMenuItem->setHover(false);

    activeMenuItem = item;

    // A disabled item cannot be selected: don't mutate it (no hover) and don't place the outline
    // behind it; just hide the outline.
    if (activeMenuItem && activeMenuItem->enabled())
    {
        activeMenuItem->setHover(true);
        menuItemOutline.setOpacity(1.f); // Restore opacity when the selection moves to another item.
        menuItemOutline.insertBefore(activeMenuItem);
        menuItemOutline.layout().setHeight(activeMenuItem->worldRect().height());
        menuItemOutline.layout().setPosition(YGEdgeLeft, 0.f);
        menuItemOutline.layout().setPosition(YGEdgeTop, activeMenuItem->layout().calculatedTop());
    }
    else
        menuItemOutline.setParent(nullptr);

    // Open/close the submenu associated with the active item (a disabled submenu does not open).
    if (auto *sm { dynamic_cast<SubMenu*>(item) }; sm && sm->submenu && sm->enabled())
        openSubMenu(sm);
    else
        closeSubMenu();
}

void MenuSurface::setOutlinePressed(bool pressed) noexcept
{
    menuItemOutline.setOpacity(pressed ? 0.8f : 1.f);
}

void MenuSurface::openSubMenu(SubMenu *item) noexcept
{
    MenuModel *model { item->submenu };

    if (!model)
        return;

    // Already showing this submenu.
    if (subMenu && subMenu->hostedContent() == model->content())
        return;

    // This surface manages two child submenu surfaces (like Firmament manages the topbar menus),
    // alternating between them so the visible submenu swaps smoothly.
    if (!m_subA) m_subA = std::make_unique<MenuSurface>();
    if (!m_subB) m_subB = std::make_unique<MenuSurface>();

    MenuSurface *next { subMenu.get() == m_subA.get() ? m_subB.get() : m_subA.get() };

    prevSubMenu = subMenu;
    subMenu.reset(next);

    next->host(model);
    next->beforeMap = [this]{
        if (prevSubMenu)
            prevSubMenu->setMapped(false);
    };

    next->openToRight(this, item);
}

void MenuSurface::closeSubMenu() noexcept
{
    if (subMenu)
        subMenu->setMapped(false);
    subMenu.reset();
}

void MenuSurface::refreshSize() noexcept
{
    const SkISize s { minContentSize() };
    layout().setWidth(s.width());
    layout().setHeight(s.height());
}

void MenuSurface::setUIScale(float scale) noexcept
{
    if (m_hostedContent)
        m_hostedContent->setUIScale(scale);

    if (m_uiScale == scale)
        return;

    m_uiScale = scale;
    slot()->layout().setPadding(YGEdgeVertical, MENU_VPADDING * scale);
    slot()->layout().setPadding(YGEdgeHorizontal, MENU_HPADDING * scale);
    layout().setMinWidth(275 * scale);
    menuItemOutline.setBorderRadius(CZBorderRadius::Make(Int32(std::round(4 * scale))));
}

void MenuSurface::openUnder(MSurface *parent, AKNode *anchorItem) noexcept
{
    // Under a top bar item: that screen's scale.
    const float scale { dynamic_cast<TopbarSurface*>(parent) ? static_cast<TopbarSurface*>(parent)->uiScale() : 1.f };
    setUIScale(scale);
    setParent(parent);
    setAnchor(MPopup::Anchor::BottomLeft);
    setGravity(MPopup::Gravity::BottomRight);

    const SkIRect r { anchorItem->worldRect() };
    setAnchorRect(SkIRect::MakeXYWH(Int32(r.x() - TOPBAR_OUTLINE_PADDING * scale), 0, r.width(), Int32(std::round(TOPBAR_HEIGHT * scale))));

    refreshSize();
    setMapped(true);
}

void MenuSurface::openToRight(MSurface *parent, AKNode *anchorItem) noexcept
{
    // A submenu: its parent menu's scale.
    if (auto *menu { dynamic_cast<MenuSurface*>(parent) })
        setUIScale(menu->m_uiScale);

    setParent(parent);
    setAnchor(MPopup::Anchor::TopRight);
    setGravity(MPopup::Gravity::BottomRight);
    setConstraintAdjustment(MPopup::FlipX | MPopup::SlideY);
    //setOffset(0, -MENU_VPADDING);
    setAnchorRect(SkIRect::MakeXYWH(
        0, anchorItem->layout().calculatedTop(),
        anchorItem->layout().calculatedWidth(),
        anchorItem->layout().calculatedHeight()));
    refreshSize();
    setMapped(true);
}

void MenuSurface::Activate(MenuItem &item) noexcept
{
    auto *menu { dynamic_cast<MenuSurface*>(item.parentWindow()) };

    const auto run = [](MenuItem *i) {
        if (i->onClicked)
            i->onClicked();

        // Dismiss the whole menu chain after activating the item.
        GetFirm()->setActiveTopbarItem(nullptr);
    };

    if (!menu)
    {
        run(&item);
        return;
    }

    if (menu->m_activating)
        return;

    menu->m_activating = true;

    // Highlight off, on, off, on (like macOS), then the click.
    static constexpr UInt32 StepMs { 60 };
    static constexpr int Steps { 4 };

    for (int i = 1; i <= Steps; i++)
    {
        CZTimer::OneShot(StepMs * i, [run, weakMenu = CZWeak<MenuSurface>(menu), weakItem = CZWeak<MenuItem>(&item), on = i % 2 == 0, last = i == Steps](CZTimer *) {
            if (!weakMenu || !weakItem)
            {
                if (weakMenu && last)
                    weakMenu->m_activating = false;
                return;
            }

            weakMenu->menuItemOutline.setOpacity(on ? 1.f : 0.f);
            weakItem->setHover(on);

            if (!last)
                return;

            weakMenu->m_activating = false;
            run(weakItem.get());
        });
    }
}
