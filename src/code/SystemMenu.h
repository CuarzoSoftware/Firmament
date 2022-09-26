#ifndef SYSTEMMENU_H
#define SYSTEMMENU_H

#include <QGraphicsDropShadowEffect>

#include "Menu.h"
#include "Action.h"
#include "Separator.h"

class SystemMenu : public Menu
{
    Q_OBJECT
public:
    SystemMenu(Firmament *firmament);


    QGraphicsDropShadowEffect shadow = QGraphicsDropShadowEffect();

    Action aboutCuarzoOSAction;

    Separator separator1;
    Action sysPrefsAction;
    Action appStoreAction;

    Separator separator2;
    Menu recentItemsMenu;
    Separator recentItemsAppsSeparator;
    Separator recentItemsDocsSeparator;

    Separator separator3;
    Action forceExitAction;

    Separator separator4;
    Action sleepAction;
    Action restartAction;
    Action shutDownAction;

    Separator separator5;
    Action lockScreenAction;
    Action closeSessionAction;

};

#endif // SYSTEMMENU_H
