#ifndef MENUSURFACE_H
#define MENUSURFACE_H

#include <Marco/Roles/MMenu.h>
#include <AK/Nodes/AKContainer.h>
#include <Core/Types.h>

using namespace CZ;

class MenuSurface final : public MMenu
{
public:
    MenuSurface() noexcept;
    AKContainer content { YGFlexDirectionColumn, false, this };
};

#endif // MENUSURFACE_H
