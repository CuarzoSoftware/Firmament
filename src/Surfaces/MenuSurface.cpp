#include <Surfaces/MenuSurface.h>
#include <Models/MenuModel.h>
#include <Nodes/MenuItemsContainer.h>
#include <Nodes/MenuItem.h>
#include <Nodes/SubMenu.h>
#include <Core/Firmament.h>
#include <cassert>

MenuSurface::MenuSurface() noexcept
{
    slot()->layout().setPadding(YGEdgeVertical, MENU_VPADDING);
    slot()->layout().setPadding(YGEdgeHorizontal, MENU_HPADDING);
    layout().setMinWidth(275);

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
    if (item == activeMenuItem)
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

void MenuSurface::openUnder(MSurface *parent, AKNode *anchorItem) noexcept
{
    setParent(parent);
    setAnchor(MPopup::Anchor::BottomLeft);
    setGravity(MPopup::Gravity::BottomRight);

    const SkIRect r { anchorItem->worldRect() };
    setAnchorRect(SkIRect::MakeXYWH(r.x() - TOPBAR_OUTLINE_PADDING, 0, r.width(), TOPBAR_HEIGHT));

    refreshSize();
    setMapped(true);
}

void MenuSurface::openToRight(MSurface *parent, AKNode *anchorItem) noexcept
{
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
