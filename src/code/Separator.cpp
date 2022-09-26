#include "Separator.h"

Separator::Separator(Firmament *firmament, hn_object *object, QWidget *parent) : Object(firmament, object, QBoxLayout::Direction::TopToBottom, parent),
    m_bar(this)
{
    layout().setContentsMargins(8,4,8,4);
    layout().setSpacing(8);

    layout().addWidget(&m_bar);
    label().setObjectName("SeparatorLabel");
    layout().addWidget(&label());


    m_bar.setFixedHeight(1);
    m_bar.setObjectName("SeparatorBar");
    label().hide();
}

hn_object_type Separator::typeHN() const
{
    return HN_OBJECT_TYPE_SEPARATOR;
}

void Separator::setLabelHN(const QString &label)
{
    this->label().setText(label);
    this->label().setVisible(!label.isEmpty());
}

QWidget &Separator::bar()
{
    return m_bar;
}
