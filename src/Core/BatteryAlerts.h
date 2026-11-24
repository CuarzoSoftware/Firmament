#ifndef BATTERYALERTS_H
#define BATTERYALERTS_H

#include <Core/Types.h>
#include <CZ/Core/CZObject.h>
#include <CZ/Services/SStatus.h>
#include <memory>

using namespace CZ;

/**
 * @brief Notifications when the battery runs low (UPower's warning levels, see SStatus::BatteryWarning).
 *
 * - Low (20 % by default): "Low Battery", how much is left.
 * - Critical (5 % by default) and Action: "Battery Critically Low", critical urgency (it stays until
 *   dismissed), replacing the low one.
 *
 * Each is shown once per discharge: plugging in closes it, and it's shown again the next time the
 * battery gets that low.
 */
class BatteryAlerts final : public CZObject
{
public:
    BatteryAlerts() noexcept;

private:
    void update() noexcept;
    std::shared_ptr<SStatus> m_status;
    std::shared_ptr<SNotificationsClient> m_notifications;
    SStatus::BatteryWarning m_shown { SStatus::BatteryWarning::None }; // The worst one notified
    UInt32 m_id { 0 };      // Its notification (0: none, or not sent yet)
    bool m_sending { false };
    std::shared_ptr<bool> m_alive { std::make_shared<bool>(true) }; // Replies after destruction are ignored
};

#endif // BATTERYALERTS_H
