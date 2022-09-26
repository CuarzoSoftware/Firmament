#ifndef MENU_H
#define MENU_H

// #include <QFrame>
// #include <QVBoxLayout>
// #include <QGraphicsDropShadowEffect>

#include "Object.h"
#include "TopBarItem.h"
#include "MenuItem.h"

class Item;
class Firmament;
class Client;

class Menu : public Object
{
public:
    Menu(Firmament *firmament, hn_object *object, QWidget *parent = nullptr);
    ~Menu();
    hn_object_type typeHN() const override;

    void setParentLocal(Object *parent = nullptr, Object *before = nullptr) override;
    void setParentHN(hn_object *parent, hn_object *before) override;
    void closeEvent(QCloseEvent *event) override;

    void setEnabledHN(hn_bool enabled) override;

    void updateChildren() override;

    TopBarItem &topBarItem();
    MenuItem &menuItem();

    bool eventFilter(QObject *object, QEvent *event) override;

    MenuItem *activeMenuItem = nullptr;


private:
    TopBarItem m_topBarItem;
    MenuItem m_menuItem;
    /*
    Firmament *p = nullptr;
    void *data = nullptr;
    TopBarItem topBarItem = TopBarItem(this);

    QVBoxLayout mainLayout = QVBoxLayout(this);
    QFrame content = QFrame(this);
    QVBoxLayout lay = QVBoxLayout(&content);
    QGraphicsDropShadowEffect menuShadow = QGraphicsDropShadowEffect();

    Client *client() const;
    void setTitle(const QString &title);
    void removeItem(Item *item);
    void addItem(Item *item, Item *before);


    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent*) override;
    */
};

#endif // MENU_H
