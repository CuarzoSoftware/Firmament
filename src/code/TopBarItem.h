#ifndef TOPBARITEM_H
#define TOPBARITEM_H

#include <QFrame>
#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>

class Menu;

class TopBarItem : public QFrame
{
    Q_OBJECT
public:
    TopBarItem(Menu *menu, QWidget *parent = nullptr);
    ~TopBarItem();

    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *e) override;

    void setActive(bool state);

    Menu *menu() const;
    QHBoxLayout &layout();

    QGraphicsDropShadowEffect textShadow = QGraphicsDropShadowEffect();

    void updateStyle();

public slots:
    void darkMode(bool mode);
private:
    Menu *m_menu = nullptr;
    QHBoxLayout m_layout;
};

#endif // TOPBARITEM_H
