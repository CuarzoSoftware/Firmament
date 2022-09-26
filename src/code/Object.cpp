#include "Object.h"
#include "Client.h"
#include "Firmament.h"

Object::Object(Firmament *firmament, hn_object *object, QBoxLayout::Direction layoutDirection, QWidget *parent) : QFrame(parent),
    opacity(this),
    m_firmament(firmament),
    m_layout(layoutDirection, this),
    m_label(this),
    m_icon(this),
    m_enabled(true),
    m_iconNormal(),
    m_iconActive()
{
    setProperty("HNObject",true);
    opacity.setOpacity(0.4);
    opacity.setEnabled(false);
    m_resource = object;

    if(object)
        hn_object_set_user_data(object, this);

    setProperty("active", false);
    setProperty("enabled", true);
    m_icon.hide();
    m_icon.setScaledContents( true );
    hide();
    setMouseTracking(true);
}

Object::~Object()
{
    setParent(nullptr);
}

void Object::setParentLocal(Object *parent, Object *before)
{
    Object *oldParent = parentHN();

    if(!parent)
    {
        setParent(nullptr);
        hide();
        m_parent = nullptr;
    }
    else
    {
        if(before)
        {
            int beforeIndex;

            if(before->typeHN() == HN_OBJECT_TYPE_MENU)
                beforeIndex = parent->layout().indexOf(&((Menu*)before)->menuItem());
            else
                beforeIndex = parent->layout().indexOf(before);

            parent->layout().insertWidget(beforeIndex, this);
        }
        else
        {
            parent->layout().addWidget(this);
        }
        show();
        m_parent = parent;

        parent->updateChildren();
    }

    if(oldParent)
    {
        oldParent->updateChildren();
    }
}

void Object::setParentHN(hn_object *parent, hn_object *before)
{
    Object *oldParent = parentHN();

    if(!parent)
    {
        if(oldParent)
            oldParent->layout().removeWidget(this);

        setParent(nullptr);
        hide();
        m_parent = nullptr;
    }
    else
    {

        Object *parentObject = (Object*)hn_object_get_user_data(parent);

        parentObject->layout().removeWidget(this);

        if(before)
        {

            int beforeIndex;

            if(hn_object_get_type(before) == HN_OBJECT_TYPE_MENU)
                beforeIndex = parentObject->layout().indexOf(&((Menu*)hn_object_get_user_data(before))->menuItem());
            else
                beforeIndex = parentObject->layout().indexOf((Object*)hn_object_get_user_data(before));

            parentObject->layout().insertWidget(beforeIndex, this);
        }
        else
        {
            parentObject->layout().addWidget(this);
        }

        parentObject->updateChildren();
        show();
        m_parent = parentObject;
    }

    if(oldParent)
    {
        oldParent->updateChildren();
    }
}

void Object::updateChildren()
{

}

void Object::setLabelHN(const QString &label)
{
    m_label.setText(label);

    if(label.isEmpty())
        m_label.hide();
    else
        m_label.show();
}

void Object::setIconHN(const hn_pixel *pixels, u_int32_t width, u_int32_t height)
{
    if(pixels)
    {
        m_iconNormal = QImage(width, height, QImage::Format_ARGB32);
        m_iconActive = QImage(width, height, QImage::Format_ARGB32);

        u_int32_t i = 0;

        for(u_int32_t x = 0; x < width; x++)
        {
            for(u_int32_t y = 0; y < height; y++)
            {
                m_iconNormal.setPixel(x,y,qRgba(80,80,80,pixels[i]));
                m_iconActive.setPixel(x,y,qRgba(255,255,255,pixels[i]));
                i++;
            }
        }

        if(property("hover").toBool() && enabledHN())
            m_icon.setPixmap(QPixmap::fromImage(m_iconActive));
        else
            m_icon.setPixmap(QPixmap::fromImage(m_iconNormal));

        m_icon.show();
        m_hasIcon = true;
    }
    else
    {
        m_icon.hide();
        m_hasIcon = false;
    }


}

void Object::setEnabledHN(hn_bool enabled)
{
    m_enabled = enabled;
}

Firmament *Object::firmament() const
{
    return m_firmament;
}

QBoxLayout &Object::layout()
{
    return m_layout;
}

hn_object *Object::resource() const
{
    return m_resource;
}

Client *Object::client() const
{
    return (Client*)hn_client_get_user_data(hn_object_get_client(m_resource));
}

QLabel &Object::label()
{
    return m_label;
}

QLabel &Object::icon()
{
    return m_icon;
}

bool Object::enabledHN() const
{
    return m_enabled;
}

Object *Object::parentHN() const
{
    return m_parent;
}
