#include "Firmament.h"
#include "TopBar.h"
#include "Menu.h"
#include "Client.h"

TopBar::TopBar(Firmament *firmament, hn_object *object, QWidget *parent) : Object(firmament, object, QBoxLayout::Direction::LeftToRight, parent)
{
    client()->addTopBar(this);
    layout().setContentsMargins(0,0,0,0);
    layout().setSpacing(FIRMAMENT_TOPBAR_ITEM_SPACING);
}

hn_object_type TopBar::typeHN() const
{
    return HN_OBJECT_TYPE_TOP_BAR;
}

TopBar::~TopBar()
{
    client()->removeTopBar(this);

    while(layout().count() > 0)
    {
        layout().itemAt(0)->widget()->setParent(nullptr);
    }

}

void TopBar::setActiveHN()
{
    client()->setActiveTopBar(this);
}

/*
void TopBar::removeMenu(Menu *menu)
{
    lay.removeWidget(&menu->topBarItem);
    repaint();
}

void TopBar::addMenu(Menu *menu, Menu *before)
{
    if(before)
        lay.insertWidget(lay.indexOf(&before->topBarItem),&menu->topBarItem);
    else
        lay.addWidget(&menu->topBarItem);

    menu->topBarItem.show();
}
*/
