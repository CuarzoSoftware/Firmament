#ifndef OPTION_H
#define OPTION_H

#include "Object.h"

class Option : public Object
{
    Q_OBJECT
public:
    Option(Firmament *firmament, hn_object *object, QBoxLayout::Direction layoutDirection, QWidget *parent = nullptr);
    hn_object_type typeHN() const override;


};

#endif // OPTION_H
