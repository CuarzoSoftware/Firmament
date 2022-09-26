#ifndef SELECT_H
#define SELECT_H

#include "Object.h"

class Select : public Object
{
    Q_OBJECT
public:
    Select(Firmament *firmament, hn_object *object, QBoxLayout::Direction layoutDirection, QWidget *parent = nullptr);
    hn_object_type typeHN() const override;
};

#endif // SELECT_H
