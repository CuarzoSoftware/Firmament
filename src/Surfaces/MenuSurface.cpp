#include <Core/Events/CZInputEvent.h>
#include <Surfaces/MenuSurface.h>

MenuSurface::MenuSurface() noexcept
{
    layout().setMinWidth(250);
    //layout().setMinHeight(400);
    content.layout().setFlexDirection(YGFlexDirectionColumn);
    content.layout().setAlignItems(YGAlignStretch);
    content.layout().setMinWidth(MENU_MIN_WIDTH);
    content.layout().setPadding(YGEdgeVertical, MENU_VPADDING);
    content.layout().setPadding(YGEdgeHorizontal, MENU_HPADDING);
}

