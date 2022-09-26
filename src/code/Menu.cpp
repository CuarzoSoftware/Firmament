#include "Firmament.h"
#include "Menu.h"
#include "Action.h"
#include "Client.h"

#include <QMouseEvent>

Menu::Menu(Firmament *firmament, hn_object *object, QWidget *parent) : Object(firmament, object, QBoxLayout::Direction::TopToBottom, parent) ,
    m_topBarItem(this),
    m_menuItem(this)
{
    setObjectName("Menu");
    layout().setContentsMargins(5,5,5,5);
    layout().setSpacing(0);
    setMinimumWidth(250);
    setMinimumHeight(26);
    //setFixedHeight(1800/2 - 22);
    menuItem().setGraphicsEffect(&opacity);

}

Menu::~Menu()
{
    setParent(nullptr);
    while(layout().count() > 0)
    {
        layout().itemAt(0)->widget()->setParent(nullptr);
    }

    setGraphicsEffect(&opacity);
    m_topBarItem.setParent(this);
    m_menuItem.setParent(this);

    label().setParent(this);
    icon().setParent(this);
}

hn_object_type Menu::typeHN() const
{
    return HN_OBJECT_TYPE_MENU;
}

void Menu::setParentLocal(Object *parent, Object *before)
{
    Object *oldParent = parentHN();

    if(!parent)
    {
        m_topBarItem.setParent(nullptr);
        m_topBarItem.hide();
        m_menuItem.setParent(nullptr);
        m_menuItem.hide();
        setParent(nullptr);
        hide();
        m_parent = nullptr;
    }
    else
    {
        if(before)
        {
            parent->layout().insertWidget(parent->layout().indexOf(before), &m_menuItem);
        }
        else
            parent->layout().addWidget(&m_menuItem);

        m_menuItem.layout().insertWidget(0,&icon());
        m_menuItem.layout().insertWidget(1,&label());
        m_topBarItem.setParent(nullptr);
        m_topBarItem.hide();

        m_parent = parent;

        if(enabledHN())
            m_topBarItem.show();

        parent->updateChildren();

    }

    if(oldParent)
        oldParent->updateChildren();

    m_topBarItem.darkMode(firmament()->darkBackground);
}

TopBarItem &Menu::topBarItem()
{
    return m_topBarItem;
}

MenuItem &Menu::menuItem()
{
    return m_menuItem;
}

bool Menu::eventFilter(QObject *object, QEvent *event)
{
    firmament()->eventFilter(object, event);

    if(activeMenuItem && event->type() == QEvent::Type::MouseMove)
    {
        QMouseEvent *e = (QMouseEvent*)event;

        for(int i = 0; i < layout().count(); i++)
        {
            Object *obj = (Object*)layout().itemAt(i)->widget();
            hn_object_type type;

            if(!obj->property("HNObject").toBool())
                type = HN_OBJECT_TYPE_MENU;
            else
                type = obj->typeHN();

            if(obj != (Object*)activeMenuItem &&
                    type != HN_OBJECT_TYPE_SEPARATOR &&
                    QRect(obj->mapToGlobal(QPoint()),obj->size()).contains(e->globalPosition().toPoint()))
            {
                activeMenuItem->menu()->close();
                return false;
            }

        }
    }

    return false;
}


void Menu::setParentHN(hn_object *parent, hn_object *before)
{
    Object *oldParent = parentHN();

    if(!parent)
    {

        if(oldParent)
        {
            oldParent->layout().removeWidget(&m_topBarItem);
            oldParent->layout().removeWidget(&m_menuItem);
        }
        m_topBarItem.setParent(nullptr);
        m_topBarItem.hide();
        m_menuItem.setParent(nullptr);
        m_menuItem.hide();
        setParent(nullptr);
        hide();
        m_parent = nullptr;
    }
    else
    {
        Object *parentObject = (Object*)hn_object_get_user_data(parent);

        if(hn_object_get_type(parent) == HN_OBJECT_TYPE_TOP_BAR)
        {
            if(before)
            {
                int beforeIndex = parentObject->layout().indexOf(&((Menu*)hn_object_get_user_data(before))->topBarItem());
                parentObject->layout().insertWidget(beforeIndex, &m_topBarItem);
            }
            else
                parentObject->layout().addWidget(&m_topBarItem);

            m_topBarItem.layout().insertWidget(0,&label());
            m_menuItem.setParent(nullptr);
            m_menuItem.hide();

            if(enabledHN())
                m_topBarItem.show();
        }
        else
        {
            if(before)
            {
                int beforeIndex;

                if(hn_object_get_type(before) == HN_OBJECT_TYPE_MENU)
                    beforeIndex = parentObject->layout().indexOf(&((Menu*)hn_object_get_user_data(before))->menuItem());
                else
                    beforeIndex = parentObject->layout().indexOf((Object*)hn_object_get_user_data(before));

                parentObject->layout().insertWidget(beforeIndex, &m_menuItem);
            }
            else
                parentObject->layout().addWidget(&m_menuItem);

            m_menuItem.layout().insertWidget(0,&icon());
            m_menuItem.layout().insertWidget(1,&label());
            m_topBarItem.setParent(nullptr);
            m_topBarItem.hide();
            m_menuItem.show();
        }

        m_parent = parentObject;
        parentObject->updateChildren();
    }

    if(oldParent)
        oldParent->updateChildren();

    m_topBarItem.darkMode(firmament()->darkBackground);
}

void Menu::closeEvent(QCloseEvent *)
{
    if(activeMenuItem)
        activeMenuItem->menu()->close();

    if(&topBarItem() == firmament()->activeTopBarItem)
        firmament()->selectedItemBackground().hide();

    topBarItem().leaveEvent(nullptr);
    menuItem().leaveEvent(nullptr);
}

void Menu::setEnabledHN(hn_bool enabled)
{
    m_enabled = enabled;

    if(parentHN())
    {
        if(parentHN()->typeHN() == HN_OBJECT_TYPE_TOP_BAR)
            topBarItem().setVisible(enabled);
        else
            opacity.setEnabled(!enabled);
    }
}

void Menu::updateChildren()
{
    Object *prev = nullptr;

    for(int i = 0; i < layout().count(); i++)
    {
        Object *obj = (Object*)layout().itemAt(i)->widget();

        if(!obj->property("HNObject").toBool())
            continue;

        if(obj->typeHN() == HN_OBJECT_TYPE_SEPARATOR)
        {
            Separator *sep = (Separator*)obj;

            if(i == 0 || i == layout().count()-1 || (prev && prev->typeHN() == HN_OBJECT_TYPE_SEPARATOR))
                sep->bar().setVisible(false);
            else
                sep->bar().setVisible(true);
        }

        prev = obj;
    }
}


