#ifndef SEPARATOR_H
#define SEPARATOR_H

#include <QObject>
#include <QWidget>

#include "Object.h"

class Firmament;

class Separator : public Object
{
    Q_OBJECT
public:
    Separator(Firmament *firmament, hn_object *object, QWidget *parent = nullptr);
    hn_object_type typeHN() const override;
    void setLabelHN(const QString &label) override;

    QWidget &bar();
private:
    QWidget m_bar;

};

#endif // SEPARATOR_H
