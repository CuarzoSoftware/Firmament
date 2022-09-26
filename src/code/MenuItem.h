#ifndef MENUITEM_H
#define MENUITEM_H

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>

class Menu;

class MenuItem : public QFrame
{
    Q_OBJECT
public:
    MenuItem(Menu *menu, QWidget *parent = nullptr);
    QHBoxLayout &layout();
    QLabel &rightArrow();
    Menu *menu() const;

    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

    void updateStyle();
private:
    QWidget m_baseWidget;
    QHBoxLayout m_baseLayout;
    Menu *m_menu = nullptr;
    QHBoxLayout m_layout;
    QLabel m_rightArrow;
};

#endif // MENUITEM_H
