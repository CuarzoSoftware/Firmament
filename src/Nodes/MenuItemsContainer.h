#ifndef MENUITEMSCONTAINER_H
#define MENUITEMSCONTAINER_H

#include <AK/Nodes/AKContainer.h>

using namespace CZ;

class MenuItemsContainer : public AKContainer
{
public:
    MenuItemsContainer(AKNode *parent = nullptr) noexcept;
};

#endif // MENUITEMSCONTAINER_H
