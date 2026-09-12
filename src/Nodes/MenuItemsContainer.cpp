#include <Nodes/MenuItemsContainer.h>
#include <Nodes/MenuItem.h>
#include <Core/Types.h>

MenuItemsContainer::MenuItemsContainer(AKNode *parent) noexcept : AKContainer(YGFlexDirectionColumn, false, parent)
{
    // This container is the content of a MenuSurface: a menu owns it and reparents it into whichever
    // of the surface's two pooled MenuSurfaces is active, so it carries the dropdown's own styling.
    layout().setAlignItems(YGAlignStretch);
    layout().setMinWidth(MENU_MIN_WIDTH);
    layout().setMinHeight(MENU_MIN_HEIGHT);

    onChildAdded.subscribe(this, [this](AKNode *, AKNode *)
    {
        for (AKNode *c : children())
        {
            auto *child { dynamic_cast<MenuItem*>(c) };
            if (child) child->siblingsChanged();
        }
    });

    onChildRemoved.subscribe(this, [this](AKNode *)
    {
        for (AKNode *c : children())
        {
            auto *child { dynamic_cast<MenuItem*>(c) };
            if (child) child->siblingsChanged();
        }
    });
}
