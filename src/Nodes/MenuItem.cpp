#include <Nodes/MenuItem.h>
#include <Surfaces/MenuSurface.h>
#include <Core/Firmament.h>
#include <Core/Types.h>
#include <Core/Events/CZPointerButtonEvent.h>

MenuItem::MenuItem(AKNode *parent) noexcept : AKContainer(YGFlexDirectionRow, false, parent)
{
    layout().setAlignItems(YGAlignCenter);
    layout().setJustifyContent(YGJustifyFlexStart);
    layout().setGap(YGGutterAll, 8.f);
}

void MenuItem::setUIScale(float scale) noexcept
{
    if (m_uiScale == scale)
        return;

    m_uiScale = scale;
    applyUIScale();
}

void MenuItem::applyUIScale() noexcept
{
    layout().setGap(YGGutterAll, 8.f * m_uiScale);
}

void MenuItem::pointerEnterEvent(const CZPointerEnterEvent &e)
{
    AKContainer::pointerEnterEvent(e);
    auto *menu { dynamic_cast<MenuSurface*>(parentWindow()) };

    if (menu)
        menu->setActiveMenuItem(this);
}

void MenuItem::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    AKContainer::pointerButtonEvent(e);

    // Dim the selection outline while the item is held, restoring it on release. Disabled items
    // have no outline, so they are ignored.
    if (e.button != BTN_LEFT || !enabled())
        return;

    if (auto *menu { dynamic_cast<MenuSurface*>(parentWindow()) })
        menu->setOutlinePressed(e.pressed);
}
