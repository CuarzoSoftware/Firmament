#include <QStyle>

#include "AppMenu.h"
#include "Firmament.h"

AppMenu::AppMenu(Firmament *firmament) : Menu(firmament, nullptr)
{
    topBarItem().setProperty("SysMenu",true);
    label().setObjectName("AppName");
    label().setText("Firmament");
    topBarItem().layout().addWidget(&label());
    this->firmament()->layout().addWidget(&topBarItem());
    label().show();
    topBarItem().show();
    this->firmament()->layout().addStretch(1);

}

