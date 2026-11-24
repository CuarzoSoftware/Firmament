#include <Core/BatteryAlerts.h>
#include <Core/Log.h>
#include <CZ/Services/SVNotificationsClient.h>

using Warning = SVStatus::BatteryWarning;

BatteryAlerts::BatteryAlerts() noexcept
{
    m_status = SVStatus::GetOrMake();
    m_notifications = SVNotificationsClient::GetOrMake();
    m_status->onChanged.subscribe(this, [this]{ update(); });

    // Not sent because no daemon was running yet (e.g. at login, before Gazette): now
    m_notifications->onChanged.subscribe(this, [this]{
        if (m_notifications->available())
            update();
    });

    // Dismissed by the user: shown again only after the next charge (or when it gets worse)
    m_notifications->onClosed.subscribe(this, [this](UInt32 id, SVNotification::CloseReason){
        if (id == m_id)
            m_id = 0;
    });

    update();
}

void BatteryAlerts::update() noexcept
{
    const Warning warning { m_status->batteryWarning };

    // Plugged in (or fine again): the warning goes, and can be shown again next time
    if (warning == Warning::None)
    {
        if (m_id)
            m_notifications->close(m_id);

        m_id = 0;
        m_shown = Warning::None;
        return;
    }

    // Only when it gets worse (Action counts as Critical: the same notification)
    const Warning level { warning == Warning::Action ? Warning::Critical : warning };

    if (UInt8(level) <= UInt8(m_shown) || m_sending)
        return;

    m_shown = level;
    const Int32 percent { std::max(m_status->batteryPercent, 0) };

    SVNotification n;
    n.appName = "Battery";
    n.replaces = m_id; // The low one becomes the critical one

    if (level == Warning::Low)
    {
        n.appIcon = "battery-low";
        n.summary = "Low Battery";
        n.body = std::to_string(percent) + "% of battery remaining. Connect to power soon.";
    }
    else
    {
        n.appIcon = "battery-caution";
        n.summary = "Battery Critically Low";
        n.body = std::to_string(percent) + "% of battery remaining. Connect to power now.";
        n.urgency = SVNotification::Urgency::Critical;
    }

    Log(CZInfo, CZLN, "{} ({}%)", n.summary, percent);
    m_sending = true;
    m_notifications->send(n, [this, alive = std::weak_ptr<bool>(m_alive)](UInt32 id){
        if (alive.expired())
            return;

        m_sending = false;
        m_id = id;

        // Not sent (no daemon yet): again when one shows up
        if (id == 0)
        {
            m_shown = Warning::None;
            return;
        }

        // Plugged in meanwhile
        if (m_shown == Warning::None && id)
        {
            m_notifications->close(id);
            m_id = 0;
        }

        update(); // Worse meanwhile
    });
}
