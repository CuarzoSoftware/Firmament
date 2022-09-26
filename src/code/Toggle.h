#ifndef TOGGLE_H
#define TOGGLE_H

#include "Object.h"

class Toggle : public Object
{
    Q_OBJECT
public:
    Toggle(Firmament *firmament, hn_object *object, QWidget *parent = nullptr);
    hn_object_type typeHN() const override;

    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QLabel m_shortcuts;
};

#endif // TOGGLE_H
