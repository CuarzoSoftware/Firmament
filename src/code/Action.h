#ifndef ACTION_H
#define ACTION_H

#include "Object.h"

class Menu;
class Firmament;

class Action : public Object
{
    Q_OBJECT
public:
    Action(Firmament *firmament, hn_object *object, QWidget *parent = nullptr);

    void setShortcuts(const char *shortcuts);
    hn_object_type typeHN() const override;
    /*
    QImage iconInactive;
    QImage iconActive;

    bool hasIcon = false;
    QLabel icon;
    QLabel text;
    QLabel shortcutsText;
    QLabel arrow;

    QHBoxLayout lay = QHBoxLayout(this);

    void setIcon(u_int8_t *pixels, u_int32_t width, u_int32_t height);
    void setChildMenu(Menu *menu);
    Menu *childMenu = nullptr;

    //void mousePressEvent(QMouseEvent *event) override;

    */

    void setEnabledHN(hn_bool enabled) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    //QVBoxLayout m_containerLayout;
    //QWidget m_container;
    QLabel m_shortcuts;
};

#endif // ACTION_H
