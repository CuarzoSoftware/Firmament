#include "Toggle.h"
#include "Firmament.h"

#include <QStyle>

Toggle::Toggle(Firmament *firmament, hn_object *object, QWidget *parent) : Object(firmament, object, QBoxLayout::Direction::LeftToRight, parent),
    m_shortcuts(this)
{
    m_shortcuts.setObjectName("Shortcuts");

    icon().setFixedSize(13,13);
    icon().setPixmap(QPixmap());

    layout().setContentsMargins(8,4,8,4);
    layout().setSpacing(4);

    layout().addWidget(&icon());
    layout().addWidget(&label());
    layout().addStretch(1);
    layout().addWidget(&m_shortcuts);

    label().show();
    icon().show();

}

hn_object_type Toggle::typeHN() const
{
    return HN_OBJECT_TYPE_TOGGLE;
}

void Toggle::enterEvent(QEnterEvent *event)
{
    setProperty("hover",true);
    style()->unpolish(this);
    style()->unpolish(&label());
    style()->unpolish(&m_shortcuts);
    style()->polish(this);
    style()->polish(&label());
    style()->polish(&m_shortcuts);
}

void Toggle::leaveEvent(QEvent *event)
{
    setProperty("hover",false);
    style()->unpolish(this);
    style()->unpolish(&label());
    style()->unpolish(&m_shortcuts);
    style()->polish(this);
    style()->polish(&label());
    style()->polish(&m_shortcuts);
}
