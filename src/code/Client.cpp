#include "Client.h"
#include "TopBar.h"
#include "Firmament.h"

Client::Client(hn_client *client, QWidget *parent) : QWidget(parent) ,
    m_appName(),
    m_topBarsLayout(this)
{
    m_topBarsLayout.setContentsMargins(0,0,0,0);
    m_topBarsLayout.setSpacing(0);

    hn_server *server = hn_client_get_server(client);
    m_firmament = (Firmament*)hn_server_get_user_data(server);
    hn_client_set_user_data(client, this);
    firmament()->addClient(this);
    hide();
}

Client::~Client()
{

    while(m_topBarsLayout.count() > 0)
    {
        m_topBarsLayout.itemAt(0)->widget()->setParent(nullptr);
    }

    firmament()->removeClient(this);
    setParent(nullptr);
}

Firmament *Client::firmament() const
{
    return m_firmament;
}

void Client::setAppName(const QString &appName)
{
    m_appName = appName;

    if(isActiveClient())
        firmament()->appMenu()->label().setText(m_appName);
}

const QString &Client::appName() const
{
    return m_appName;
}

void Client::addTopBar(TopBar *topBar)
{
    m_topBarsLayout.addWidget(topBar);
}

void Client::removeTopBar(TopBar *topBar)
{
    topBar->setParent(nullptr);

    if(m_activeTopBar == topBar)
        m_activeTopBar = nullptr;
}

void Client::setActiveTopBar(TopBar *topBar)
{
    if(m_activeTopBar)
        m_activeTopBar->hide();

    m_activeTopBar = topBar;
    m_activeTopBar->show();
}

TopBar *Client::activeTopBar() const
{
    return m_activeTopBar;
}

bool Client::isActiveClient() const
{
    return this == firmament()->activeClient();
}
