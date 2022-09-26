#ifndef TOPBAR_H
#define TOPBAR_H

#include <QHBoxLayout>
#include "Object.h"
#include "TopBarItem.h"

class Firmament;
class Menu;

class TopBar : public Object
{
    Q_OBJECT
public:
    explicit TopBar(Firmament *firmament, hn_object *object, QWidget *parent = nullptr);
    hn_object_type typeHN() const override;
    ~TopBar();

    void setActiveHN() override;


};

#endif // TOPBAR_H
