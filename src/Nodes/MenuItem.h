#ifndef MENUITEM_H
#define MENUITEM_H

#include <AK/Nodes/AKContainer.h>
#include <AK/Nodes/AKText.h>

using namespace CZ;

class MenuItem : public AKContainer
{
public:
    MenuItem(AKNode *parent = nullptr) noexcept;
    virtual void setHover(bool hover) noexcept = 0;
    void pointerEnterEvent(const CZPointerEnterEvent &e) override;
};

#endif // MENUITEM_H
