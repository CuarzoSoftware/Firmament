#ifndef OBJECT_H
#define OBJECT_H

#include <Heaven-Server.h>
#include <QFrame>
#include <QBoxLayout>
#include <QLabel>
#include <QGraphicsOpacityEffect>


class Firmament;
class Client;

class Object : public QFrame
{
    Q_OBJECT
public:
    explicit Object(Firmament *firmament, hn_object *object, QBoxLayout::Direction layoutDirection, QWidget *parent = nullptr);
    virtual ~Object();

    virtual void setParentLocal(Object *parent = nullptr, Object *before = nullptr);
    virtual void setParentHN(hn_object *parent, hn_object *before);
    virtual void updateChildren();
    virtual void setLabelHN(const QString &label);
    void setIconHN(const hn_pixel *pixels, u_int32_t width, u_int32_t height);
    virtual void setEnabledHN(hn_bool enabled);
    virtual void setActiveHN() {};
    virtual void setShortcuts(const char *shortcuts){};

    Firmament *firmament() const;
    hn_object *resource() const;
    Client *client() const;
    QBoxLayout &layout();
    QLabel &label();
    QLabel &icon();
    bool enabledHN() const;
    Object *parentHN() const;

    virtual hn_object_type typeHN() const = 0;

    QGraphicsOpacityEffect opacity;

private:
    Firmament *m_firmament = nullptr;
    hn_object *m_resource = nullptr;
    QBoxLayout m_layout;
    QLabel m_label;
    QLabel m_icon;

protected:
    bool m_hasIcon = false;
    bool m_enabled;
    Object *m_parent = nullptr;
    QImage m_iconNormal;
    QImage m_iconActive;


};

#endif // OBJECT_H
