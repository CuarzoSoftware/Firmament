#ifndef FIRMAMENT_H
#define FIRMAMENT_H

#include <Heaven/Heaven-Server.h>
#include <QHBoxLayout>
#include <QWidget>
#include <QLinearGradient>

#include "SharedIcons.h"
#include "SystemMenu.h"
#include "AppMenu.h"

#define FIRMAMENT_SHADOW_RADIUS 24
#define FIRMAMENT_TOPBAR_ITEM_SPACING 21
#define FIRMAMENT_TOPBAR_ITEM_BACKGROUND_MARGIN 15

class Client;
class QHBoxLayout;

class Firmament : public QWidget
{
    Q_OBJECT

public:
    Firmament(QWidget *parent = nullptr);
    ~Firmament();

    void addClient(Client *client);
    void removeClient(Client *client);

    void setActiveClientHN(hn_client *client, hn_client_pid pid);
    Client *activeClient() const;

    SharedIcons sharedIcons = SharedIcons();

    QFrame &selectedItemBackground();
    QHBoxLayout &layout();
    SystemMenu *systemMenu();
    AppMenu *appMenu();

    TopBarItem *activeTopBarItem = nullptr;

    bool darkBackground = true;
    void setDarkBackgroundMode(bool mode);
    QLinearGradient barGradient;

    bool eventFilter(QObject *object, QEvent *event) override;

protected:
    void paintEvent(QPaintEvent *e) override;

signals:
    void darkModeChanged(bool mode);

public slots:
    void heavenRequest();

private:
    hn_server *m_heaven;
    QFrame m_selectedItemBackground;
    QHBoxLayout m_layout;
    SystemMenu *m_systemMenu;
    AppMenu *m_appMenu;

    Client *m_activeClient;
};
#endif // FIRMAMENT_H
