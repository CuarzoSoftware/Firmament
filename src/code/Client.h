#ifndef CLIENT_H
#define CLIENT_H

#include <Heaven/Heaven-Server.h>
#include <QWidget>
#include <QHBoxLayout>

class Firmament;
class TopBar;

class Client : public QWidget
{
    Q_OBJECT
public:
    explicit Client(hn_client *client, QWidget *parent = nullptr);
    ~Client();

    Firmament *firmament() const;

    void setAppName(const QString &appName);
    const QString &appName() const;

    void addTopBar(TopBar *topBar);
    void removeTopBar(TopBar *topBar);

    void setActiveTopBar(TopBar *topBar);
    TopBar *activeTopBar() const;

    bool isActiveClient() const;

private:
    Firmament *m_firmament = nullptr;
    QString m_appName;
    TopBar *m_activeTopBar = nullptr;
    QHBoxLayout m_topBarsLayout;

};

#endif // CLIENT_H
