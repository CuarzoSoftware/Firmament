#include "Option.h"

Option::Option(Firmament *firmament, hn_object *object, QBoxLayout::Direction layDir, QWidget *parent) : Object(firmament, object, layDir, parent)
{

}

hn_object_type Option::typeHN() const
{
    return HN_OBJECT_TYPE_OPTION;
}
