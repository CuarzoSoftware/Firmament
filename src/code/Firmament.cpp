#include <QScreen>
#include <QPainter>
#include <QApplication>
#include <QSocketNotifier>
#include <QEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QStyle>

#include "Client.h"
#include "Firmament.h"

#include "TopBar.h"
#include "TopBarItem.h"
#include "Menu.h"
#include "Action.h"
#include "Toggle.h"
#include "Select.h"
#include "Option.h"
#include "Separator.h"



/* CLIENT REQUESTS */

static void client_connected(hn_client *client)
{
    Client *clientHN = new Client(client);

    if(!clientHN->firmament()->activeClient())
        clientHN->firmament()->setActiveClientHN(client, 1);
}

static void client_set_app_name(hn_client *client, const char *app_name)
{
    Client *clientHN = (Client*)hn_client_get_user_data(client);
    clientHN->setAppName(QString::fromUtf8(app_name));
}

static void client_send_custom_request(hn_client *, void *, u_int32_t) {}

static void client_disconnected(hn_client *client)
{
    delete (Client*)hn_client_get_user_data(client);
}

static void object_create(hn_client *client, hn_object *object)
{
    HN_UNUSED(client);

    Firmament *firmament = (Firmament*)hn_server_get_user_data(hn_client_get_server(client));

    switch(hn_object_get_type(object))
    {
        case HN_OBJECT_TYPE_TOP_BAR:
            new TopBar(firmament, object);
        break;
        case HN_OBJECT_TYPE_MENU:
            new Menu(firmament, object);
        break;
        case HN_OBJECT_TYPE_ACTION:
            new Action(firmament, object);
        break;
        case HN_OBJECT_TYPE_TOGGLE:
            new Toggle(firmament, object);
        break;
        case HN_OBJECT_TYPE_SELECT:
            new Select(firmament, object, QBoxLayout::Direction::TopToBottom);
        break;
        case HN_OBJECT_TYPE_OPTION:
            new Option(firmament, object, QBoxLayout::Direction::LeftToRight);
        break;
        case HN_OBJECT_TYPE_SEPARATOR:
            new Separator(firmament, object);
        break;
    }
}

static void object_set_parent(hn_client *, hn_object *object, hn_object *parent, hn_object *before)
{
    ((Object*)hn_object_get_user_data(object))->setParentHN(parent, before);
}

static void object_set_label(hn_client *, hn_object *object, const char *label)
{
    ((Object*)hn_object_get_user_data(object))->setLabelHN(label);
}

static void object_set_icon(hn_client *, hn_object *object, const hn_pixel *pixels, u_int32_t width, u_int32_t height)
{
    ((Object*)hn_object_get_user_data(object))->setIconHN(pixels, width, height);
}

static void object_set_shortcuts(hn_client *, hn_object *object, const char *shortcuts)
{
    ((Object*)hn_object_get_user_data(object))->setShortcuts(shortcuts);
}

static void object_set_enabled(hn_client *, hn_object *object, hn_bool enabled)
{
    ((Object*)hn_object_get_user_data(object))->setEnabledHN(enabled);
}

static void object_set_checked(hn_client *client, hn_object *object, hn_bool checked)
{

}

static void object_set_active(hn_client *, hn_object *object)
{
    ((Object*)hn_object_get_user_data(object))->setActiveHN();
}

static void object_destroy(hn_client *client, hn_object *object)
{

    if(!hn_object_get_user_data(object))
        return;

    switch(hn_object_get_type(object))
    {
        case HN_OBJECT_TYPE_TOP_BAR:
            delete ((TopBar*)hn_object_get_user_data(object));
        break;
        case HN_OBJECT_TYPE_MENU:
            delete ((Menu*)hn_object_get_user_data(object));
        break;
        case HN_OBJECT_TYPE_ACTION:
            delete ((Action*)hn_object_get_user_data(object));
        break;
        case HN_OBJECT_TYPE_TOGGLE:
            delete ((Toggle*)hn_object_get_user_data(object));
        break;
        case HN_OBJECT_TYPE_SELECT:
            delete ((Select*)hn_object_get_user_data(object));
        break;
        case HN_OBJECT_TYPE_OPTION:
            delete ((Option*)hn_object_get_user_data(object));
        break;
        case HN_OBJECT_TYPE_SEPARATOR:
            delete ((Separator*)hn_object_get_user_data(object));
        break;
    }

    hn_object_set_user_data(object, NULL);

}

/* COMPOSITOR REQUESTS */

void compositor_connected(hn_compositor *compositor)
{
    ((Firmament*)hn_server_get_user_data(hn_compositor_get_server(compositor)))->show();
}

void compositor_set_active_client(hn_compositor *compositor, hn_client *client, hn_client_pid pid)
{
    ((Firmament*)hn_server_get_user_data(hn_compositor_get_server(compositor)))->setActiveClientHN(client, pid);
}

void compositor_send_custom_request(hn_compositor *compositor, void *data, u_int32_t size)
{
    Firmament *firmament = (Firmament*)hn_server_get_user_data(hn_compositor_get_server(compositor));

    struct BarGradient
    {
        hn_message_id id;
        hn_pixel pixels[16];
    };

    BarGradient *msg = (BarGradient*)data;

    if(msg->id == 0 && size == sizeof(struct BarGradient))
    {
        firmament->barGradient.setColorAt(0.0, QColor(msg->pixels[0],msg->pixels[1],msg->pixels[2]));
        firmament->barGradient.setColorAt(0.25, QColor(msg->pixels[4],msg->pixels[5],msg->pixels[6]));
        firmament->barGradient.setColorAt(0.75, QColor(msg->pixels[8],msg->pixels[9],msg->pixels[10]));
        firmament->barGradient.setColorAt(1.0, QColor(msg->pixels[12],msg->pixels[13],msg->pixels[14]));
    }

    if((int(msg->pixels[0])+int(msg->pixels[1])+int(msg->pixels[2])+int(msg->pixels[4])+int(msg->pixels[5])+int(msg->pixels[6]))/6 < 150)
        firmament->setDarkBackgroundMode(true);
    else
        firmament->setDarkBackgroundMode(false);

    firmament->repaint();
}

void compositor_disconnected(hn_compositor *compositor)
{
    ((Firmament*)hn_server_get_user_data(hn_compositor_get_server(compositor)))->hide();
}

static hn_server_requests_interface requests_interface =
{
    &client_connected,
    &client_set_app_name,
    &client_send_custom_request,
    &client_disconnected,

    &object_create,
    &object_set_parent,
    &object_set_label,
    &object_set_icon,
    &object_set_shortcuts,
    &object_set_enabled,
    &object_set_checked,
    &object_set_active,
    &object_destroy,

    &compositor_connected,
    &compositor_set_active_client,
    &compositor_send_custom_request,
    &compositor_disconnected

};

Firmament::Firmament(QWidget *parent) : QWidget(parent),
    m_selectedItemBackground(this),
    m_layout(this),
    m_activeClient(nullptr)
{
    hide();
    move(0,0);
    layout().setContentsMargins(FIRMAMENT_TOPBAR_ITEM_SPACING,0,FIRMAMENT_TOPBAR_ITEM_SPACING,0);
    layout().setSpacing(FIRMAMENT_TOPBAR_ITEM_SPACING);

    barGradient = QLinearGradient(0,0,screen()->virtualSize().width(),0);

    m_systemMenu = new SystemMenu(this);
    m_appMenu = new AppMenu(this);

    setDarkBackgroundMode(false);

    setWindowTitle("LGlobalMenu");

    //setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setFixedSize(screen()->virtualSize().width(),24);

    selectedItemBackground().hide();
    selectedItemBackground().setFixedSize(100,height());
    selectedItemBackground().setObjectName("SelectedItemBackground");

    setMouseTracking(true);

    m_heaven = hn_server_create(NULL, this, &requests_interface);

    if(!m_heaven)
    {
        qDebug() << "Could not create heaven server.";
        exit(1);
    }

    QSocketNotifier *sn = new QSocketNotifier(hn_server_get_fd(m_heaven), QSocketNotifier::Type::Read, this);
    sn->setEnabled(true);
    connect(sn, &QSocketNotifier::activated, this, &Firmament::heavenRequest);
    sn->moveToThread(this->thread());

}

Firmament::~Firmament()
{
}

void Firmament::addClient(Client *client)
{
    m_layout.insertWidget(2, client);
}

void Firmament::removeClient(Client *client)
{
    m_layout.removeWidget(client);

    if(client == m_activeClient)
    {
        m_activeClient = nullptr;
        m_appMenu->label().setText("Firmament");
    }
}


QRegularExpression proccessNameRegex("^.*\t(.*)");

QString get_process_name_by_pid(int pid)
{
    char path[32];
    char data[48];

    sprintf(path, "/proc/%d/status",pid);

    FILE* f = fopen(path,"r");

    if(f)
    {
        size_t size;
        size = fread(data, sizeof(char), 48, f);
        fclose(f);
        if(size > 0)
        {
            data[47] = '\0';
            return proccessNameRegex.match(data).captured(1);
        }

    }

    return QString("Firmament");
}

void Firmament::setActiveClientHN(hn_client *client, hn_client_pid pid)
{
    if(m_activeClient)
    {
        m_activeClient->hide();
        m_activeClient = nullptr;
    }

    if(!client)
    {
        QString name = get_process_name_by_pid(pid);
        m_appMenu->label().setText(name);
    }
    else
    {
        m_activeClient = (Client*)hn_client_get_user_data(client);
        m_activeClient->show();
        QString name = m_activeClient->appName();
        m_appMenu->label().setText(name);

    }
}

Client *Firmament::activeClient() const
{
    return m_activeClient;
}

QFrame &Firmament::selectedItemBackground()
{
    return m_selectedItemBackground;
}

QHBoxLayout &Firmament::layout()
{
    return m_layout;
}

SystemMenu *Firmament::systemMenu()
{
    return m_systemMenu;
}

AppMenu *Firmament::appMenu()
{
    return m_appMenu;
}

void Firmament::setDarkBackgroundMode(bool mode)
{

    if(mode)
    {
        systemMenu()->icon().setPixmap(sharedIcons.cuarzoWhite);
    }
    else
    {
        systemMenu()->icon().setPixmap(sharedIcons.cuarzo);

    }

    systemMenu()->shadow.setEnabled(mode);
    darkBackground = mode;
    setProperty("DarkBackground", mode);
    style()->unpolish(&m_selectedItemBackground);
    style()->polish(&m_selectedItemBackground);
    emit darkModeChanged(mode);
}

bool Firmament::eventFilter(QObject *object, QEvent *event)
{
    if(activeTopBarItem && event->type() == QEvent::Type::MouseMove)
    {
        QMouseEvent *e = (QMouseEvent*)event;

        if(activeTopBarItem != &systemMenu()->topBarItem() &&
                QRect(systemMenu()->topBarItem().mapToGlobal(QPoint()),systemMenu()->topBarItem().size()).contains(e->globalPosition().toPoint()))
        {
            activeTopBarItem->menu()->close();
            activeTopBarItem = &systemMenu()->topBarItem();
            activeTopBarItem->mousePressEvent(nullptr);
            return false;
        }

        if(activeTopBarItem != &appMenu()->topBarItem() &&
                QRect(appMenu()->topBarItem().mapToGlobal(QPoint()),appMenu()->topBarItem().size()).contains(e->globalPosition().toPoint()))
        {
            activeTopBarItem->menu()->close();
            activeTopBarItem = &appMenu()->topBarItem();
            activeTopBarItem->mousePressEvent(nullptr);
            return false;
        }

        if(activeClient() && activeClient()->activeTopBar())
        {
            for(int i = 0; i < activeClient()->activeTopBar()->layout().count(); i++)
            {
                TopBarItem *obj = (TopBarItem*)activeClient()->activeTopBar()->layout().itemAt(i)->widget();

                if(!obj->menu()->enabledHN())
                    continue;

                if(obj != activeTopBarItem &&
                        QRect(obj->mapToGlobal(QPoint()),obj->size()).contains(e->globalPosition().toPoint()))
                {
                    activeTopBarItem->menu()->close();
                    activeTopBarItem = obj;
                    activeTopBarItem->mousePressEvent(nullptr);
                    return false;
                }

            }
        }
    }

    return false;
}

void Firmament::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);
    painter.fillRect(this->geometry(), barGradient);

    if(!darkBackground)
    {
        painter.fillRect(this->geometry(),QBrush(QColor(255,255,255,80)));
    }
    else
    {
        painter.fillRect(this->geometry(),QBrush(QColor(0,0,0,20)));
    }
}


void Firmament::heavenRequest()
{
    hn_server_dispatch_requests(m_heaven, 0);
}

 ge
