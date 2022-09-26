#include "MenuItem.h"
#include "Menu.h"
#include "Firmament.h"
#include "SharedIcons.h"

#include <QScreen>
#include <QStyle>

MenuItem::MenuItem(Menu *menu, QWidget *parent) : QFrame(parent) ,
    m_menu(menu),
    m_layout(nullptr),
    m_rightArrow(this)
{
    m_baseLayout.setSpacing(0);
    m_baseLayout.setContentsMargins(0,0,0,0);
    setLayout(&m_baseLayout);
    m_baseLayout.addWidget(&m_baseWidget);
    m_baseWidget.setLayout(&layout());
    m_baseWidget.setObjectName("MenuItem");

    rightArrow().setFixedSize(12,12);
    rightArrow().setScaledContents(true);
    rightArrow().setPixmap(this->menu()->firmament()->sharedIcons.rightArrow);
    layout().setContentsMargins(8,4,8,4);
    layout().setSpacing(6);
    layout().addStretch(1);
    layout().addWidget(&rightArrow());
}

QHBoxLayout &MenuItem::layout()
{
    return m_layout;
}

QLabel &MenuItem::rightArrow()
{
    return m_rightArrow;
}

Menu *MenuItem::menu() const
{
    return m_menu;
}

void MenuItem::enterEvent(QEnterEvent *event)
{
    if(!menu()->enabledHN())
        return;
    setProperty("hover",true);
    m_baseWidget.setProperty("hover",true);
    updateStyle();
    rightArrow().setPixmap(menu()->firmament()->sharedIcons.rightArrowWhite);

    Menu *par = (Menu*)menu()->parentHN();
    par->activeMenuItem = this;
    menu()->installEventFilter(par);
    menu()->setParent(this);
    menu()->setWindowFlags(Qt::Popup | Qt::BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus);
    menu()->move(mapToGlobal(QPoint()) + QPoint(width(),0));

    menu()->show();

    if(menu()->pos().x() + menu()->width() > screen()->virtualSize().width())
        menu()->move(mapToGlobal(QPoint()) - QPoint(menu()->width(),0));


    /*
    style()->unpolish(&text);
    style()->polish(this);
    style()->polish(&text);

    if(hasIcon)
        icon.setPixmap(QPixmap::fromImage(iconActive));
        */
}

void MenuItem::leaveEvent(QEvent *event)
{
    setProperty("hover",false);
    m_baseWidget.setProperty("hover",false);
    updateStyle();
}

void MenuItem::mousePressEvent(QMouseEvent *event)
{

}

void MenuItem::updateStyle()
{
    style()->unpolish(this);
    style()->unpolish(&menu()->label());
    style()->polish(this);
    style()->polish(&menu()->label());
    rightArrow().setPixmap(menu()->firmament()->sharedIcons.rightArrow);
}
