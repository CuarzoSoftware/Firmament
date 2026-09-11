#include <Nodes/MenuItem.h>
#include <Surfaces/MenuSurface.h>
#include <Core/Firmament.h>
#include <Core/Types.h>

MenuItem::MenuItem(AKNode *parent) noexcept : AKContainer(YGFlexDirectionRow, false, parent)
{
    layout().setAlignItems(YGAlignCenter);
    layout().setJustifyContent(YGJustifyFlexStart);
    layout().setGap(YGGutterAll, 8.f);
}

void MenuItem::pointerEnterEvent(const CZPointerEnterEvent &e)
{
    AKContainer::pointerEnterEvent(e);
    auto *menu { dynamic_cast<MenuSurface*>(parentWindow()) };

    if (menu)
        menu->setActiveMenuItem(this);
}
