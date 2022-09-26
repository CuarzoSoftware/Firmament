#include "Action.h"
#include <QRegularExpression>
#include <QStyle>

Action::Action(Firmament *firmament, hn_object *object, QWidget *parent) : Object(firmament, object, QBoxLayout::Direction::LeftToRight, parent),
    //m_containerLayout(nullptr),
    //m_container(nullptr),
    m_shortcuts(this)
{
    /*
    m_container.setLayout(&layout());
    setLayout(&m_containerLayout);
    m_containerLayout.setSpacing(0);
    m_containerLayout.setContentsMargins(0,0,0,0);
    m_containerLayout.addWidget(&m_container);
    */

    setGraphicsEffect(&opacity);

    icon().setFixedSize(13,13);
    layout().setContentsMargins(8,4,8,4);
    layout().setSpacing(4);
    layout().addWidget(&icon());
    layout().addWidget(&label());
    layout().addStretch(1);
    layout().addWidget(&m_shortcuts);
    label().show();
    m_shortcuts.setObjectName("Shortcuts");
    m_shortcuts.hide();

    setFixedHeight(22);

}

QRegularExpression shortcutsRegex("[(.*)]");


void Action::setShortcuts(const char *shortc)
{
    QString shortcuts = QString::fromUtf8(shortc);

    shortcuts = shortcuts.trimmed()
            .toLower()
            .replace(" ", ",")
            .replace("++","PLUS")
            .remove("+")
            .replace("ctrl","ʌ")
            .replace("shift","⇧")
            .replace("esc","⎋")
            .replace("alt","⌥")
            .replace("up","↑")
            .replace("left","←")
            .replace("right","→")
            .replace("down","↓")
            .replace("PLUS","+")
            .replace("home","⇱")
            .replace("del","⌦")
            .toUpper();
//+⌘⇧⎋⌥↑←→↓⇱⌦


    if(shortcuts.isEmpty())
    {
        m_shortcuts.hide();
    }
    else
    {
        QRegularExpressionMatch match = shortcutsRegex.match(shortcuts);

        m_shortcuts.setText(shortcuts);//match.captured(1).trimmed());
        m_shortcuts.show();
    }
}

hn_object_type Action::typeHN() const
{
    return HN_OBJECT_TYPE_ACTION;
}

void Action::setEnabledHN(hn_bool enabled)
{
    if(!enabled)
    {
        leaveEvent(nullptr);
    }

    opacity.setEnabled(!enabled);
    m_enabled = enabled;
}

void Action::enterEvent(QEnterEvent *event)
{
    if(!enabledHN())
        return;

    setProperty("hover",true);
    style()->unpolish(this);
    style()->unpolish(&label());
    style()->unpolish(&m_shortcuts);
    style()->polish(this);
    style()->polish(&label());
    style()->polish(&m_shortcuts);

    if(m_hasIcon)
        icon().setPixmap(QPixmap::fromImage(m_iconActive));

}

void Action::leaveEvent(QEvent *event)
{
    if(!enabledHN())
        return;

    setProperty("hover",false);
    style()->unpolish(this);
    style()->unpolish(&label());
    style()->unpolish(&m_shortcuts);
    style()->polish(this);
    style()->polish(&label());
    style()->polish(&m_shortcuts);

    if(m_hasIcon)
        icon().setPixmap(QPixmap::fromImage(m_iconNormal));
}

