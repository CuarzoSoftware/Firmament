#include "SystemMenu.h"
#include "Firmament.h"

SystemMenu::SystemMenu(Firmament *firmament) : Menu(firmament, nullptr),
    aboutCuarzoOSAction(firmament, nullptr, this),

    separator1(firmament, nullptr, this),
    sysPrefsAction(firmament, nullptr, this),
    appStoreAction(firmament, nullptr, this),

    separator2(firmament, nullptr, this),
    recentItemsMenu(firmament, nullptr, this),
    recentItemsAppsSeparator(firmament, nullptr, this),
    recentItemsDocsSeparator(firmament, nullptr, this),

    separator3(firmament, nullptr, this),
    forceExitAction(firmament, nullptr, this),

    separator4(firmament, nullptr, this),
    sleepAction(firmament, nullptr, this),
    restartAction(firmament, nullptr, this),
    shutDownAction(firmament, nullptr, this),

    separator5(firmament, nullptr, this),
    lockScreenAction(firmament, nullptr, this),
    closeSessionAction(firmament, nullptr, this)
{
    shadow.setBlurRadius(12);
    shadow.setOffset(0,0);
    shadow.setEnabled(false);
    shadow.setColor(QColor(0,0,0,100));
    icon().setGraphicsEffect(&shadow);
    icon().setFixedSize(15,15);
    icon().setPixmap(QPixmap(":/img/cuarzo-logo.png"));
    topBarItem().layout().addWidget(&icon());
    topBarItem().setFixedWidth(16);
    firmament->layout().addWidget(&topBarItem());
    icon().show();
    topBarItem().show();
    setMinimumWidth(300);

    aboutCuarzoOSAction.setLabelHN("About Cuarzo OS");
    sysPrefsAction.setLabelHN("System Preferences...");
    appStoreAction.setLabelHN("App Store...");
    recentItemsMenu.setLabelHN("Recent Items");
    forceExitAction.setLabelHN("Force Exit Crystals");
    forceExitAction.setShortcuts("⇧ʌ⎋");
    sleepAction.setLabelHN("Sleep");
    restartAction.setLabelHN("Restart");
    shutDownAction.setLabelHN("Shut Down");
    lockScreenAction.setLabelHN("Lock Screen");
    closeSessionAction.setLabelHN("Close Session");
    closeSessionAction.setShortcuts("⇧ʌQ");


    aboutCuarzoOSAction.setParentLocal(this);

    separator1.setParentLocal(this);
    sysPrefsAction.setParentLocal(this);
    appStoreAction.setParentLocal(this);

    separator2.setParentLocal(this);
    recentItemsMenu.setParentLocal(this);
    recentItemsAppsSeparator.setLabelHN("Applications");
    recentItemsDocsSeparator.setLabelHN("Documents");
    recentItemsAppsSeparator.setParentLocal(&recentItemsMenu);
    recentItemsDocsSeparator.setParentLocal(&recentItemsMenu);

    separator3.setParentLocal(this);
    forceExitAction.setParentLocal(this);

    separator4.setParentLocal(this);
    sleepAction.setParentLocal(this);
    restartAction.setParentLocal(this);
    shutDownAction.setParentLocal(this);

    separator5.setParentLocal(this);
    lockScreenAction.setParentLocal(this);
    closeSessionAction.setParentLocal(this);
    /*
    systemMenu.addItem(&aboutCuarzoOSAction, nullptr);
    systemMenu.addItem(new Separator(this), nullptr);
    systemMenu.addItem(&sysPrefsAction, nullptr);
    systemMenu.addItem(&appStoreAction, nullptr);
    systemMenu.addItem(new Separator(this), nullptr);
    systemMenu.addItem(&recentItemsAction, nullptr);
    systemMenu.addItem(new Separator(this), nullptr);
    systemMenu.addItem(&forceExitAction, nullptr);
    systemMenu.addItem(new Separator(this), nullptr);
    systemMenu.addItem(&sleepAction, nullptr);
    systemMenu.addItem(&restartAction, nullptr);
    systemMenu.addItem(&shutDownAction, nullptr);
    systemMenu.addItem(new Separator(this), nullptr);
    systemMenu.addItem(&lockScreenAction, nullptr);
    systemMenu.addItem(&closeSessionAction, nullptr);
    systemMenu.mainLayout.setContentsMargins(6,1,P_SHADOW_RADIUS,P_SHADOW_RADIUS);*/
}
