#include "Select.h"

Select::Select(Firmament *firmament, hn_object *object, QBoxLayout::Direction layDir, QWidget *parent) : Object(firmament, object, layDir, parent)
{

}

hn_object_type Select::typeHN() const
{
    return HN_OBJECT_TYPE_SELECT;
}
