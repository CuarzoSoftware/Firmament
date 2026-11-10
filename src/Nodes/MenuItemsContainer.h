#ifndef MENUITEMSCONTAINER_H
#define MENUITEMSCONTAINER_H

#include <AK/Nodes/AKContainer.h>

using namespace CZ;

class MenuItemsContainer : public AKContainer
{
public:
    MenuItemsContainer(AKNode *parent = nullptr) noexcept;

    /// Applies a UI scale to the container and every item (also to items added later).
    void setUIScale(float scale) noexcept;
    float uiScale() const noexcept { return m_uiScale; }

private:
    float m_uiScale { 1.f };
};

#endif // MENUITEMSCONTAINER_H
