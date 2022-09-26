#include "TopBarItem.h"
#include "Menu.h"
#include "Firmament.h"
#include <QApplication>
#include <QStyle>
#include <QMouseEvent>
#include "Client.h"
#include "TopBar.h"
#include <QGraphicsEffect>

TopBarItem::TopBarItem(Menu *menu, QWidget *parent) : QFrame(parent),
    m_menu(menu),
    m_layout(this)
{
    textShadow.setBlurRadius(12);
    textShadow.setOffset(0,0);
    textShadow.setColor(QColor(0,0,0,100));
    textShadow.setEnabled(false);
    menu->label().setGraphicsEffect(&textShadow);

    layout().setSpacing(0);
    layout().setContentsMargins(0,0,0,0);
    setProperty("TBI",true);

    connect(menu->firmament(), &Firmament::darkModeChanged, this, &TopBarItem::darkMode);

    /*
    lay.setContentsMargins(4,2,4,2);
    lay.setSpacing(0);
    lay.setAlignment(Qt::AlignmentFlag::AlignCenter);
    lay.addWidget(&icon);
    lay.addWidget(&text);
    icon.hide();
    setMouseTracking(true);
    setProperty("active",false);
            */
}

TopBarItem::~TopBarItem()
{
    //disconnect(menu()->firmament(), &Firmament::darkModeChanged, this, &TopBarItem::darkMode);
}

void TopBarItem::mousePressEvent(QMouseEvent *)
{
    if(!menu()->enabledHN())
        return;

    menu()->firmament()->selectedItemBackground().move(mapToGlobal(QPoint()).x() - FIRMAMENT_TOPBAR_ITEM_BACKGROUND_MARGIN, 0);
    menu()->firmament()->selectedItemBackground().setFixedWidth(width() + FIRMAMENT_TOPBAR_ITEM_BACKGROUND_MARGIN*2);
    menu()->firmament()->selectedItemBackground().show();

    menu()->firmament()->activeTopBarItem = this;
    menu()->installEventFilter(menu()->firmament());

    menu()->setParent(&menu()->firmament()->selectedItemBackground());
    menu()->setWindowFlags(Qt::Popup | Qt::BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus);
    menu()->move(menu()->firmament()->selectedItemBackground().mapToGlobal(QPoint()).x(),menu()->firmament()->height());
    menu()->show();
}

void TopBarItem::enterEvent(QEnterEvent *)
{

}

void TopBarItem::leaveEvent(QEvent *)
{

}

void TopBarItem::mouseMoveEvent(QMouseEvent *e)
{

}

void TopBarItem::setActive(bool state)
{
    setProperty("active",state);
    updateStyle();
}

Menu *TopBarItem::menu() const
{
    return m_menu;
}

QHBoxLayout &TopBarItem::layout()
{
    return m_layout;
}

void TopBarItem::updateStyle()
{
    style()->unpolish(this);
    style()->polish(this);
    style()->unpolish(&menu()->label());
    style()->polish(&menu()->label());
}

void TopBarItem::darkMode(bool mode)
{
    if(mode && (property("SysMenu").toBool() || (menu()->parentHN() && menu()->parentHN()->typeHN() == HN_OBJECT_TYPE_TOP_BAR)))
        textShadow.setEnabled(true);
    else
    {
        textShadow.setEnabled(false);
    }

    updateStyle();
}
