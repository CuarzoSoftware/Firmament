#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <Core/Types.h>
#include <CZ/Core/CZObject.h>
#include <CZ/Core/CZSignal.h>
#include <memory>
#include <string>

namespace CZ { class CZBus; }
struct sd_bus_slot;

using namespace CZ;

/**
 * @brief The notifications daemon (Gazette, org.cuarzo.Gazette on the user bus), for the bars'
 *        notifications button.
 *
 * Tracks whether it's running and its state (StateChanged), and toggles its notification center.
 */
class Notifications final : public CZObject
{
public:
    Notifications() noexcept;
    ~Notifications() noexcept;

    /// Gazette is running (the button is shown).
    bool available() const noexcept { return m_available; }

    /// Its notification center is shown.
    bool centerShown() const noexcept { return m_centerShown; }

    /// Do Not Disturb is on.
    bool doNotDisturb() const noexcept { return m_dnd; }

    /// Notifications in the center.
    UInt32 count() const noexcept { return m_count; }

    /// Shows / hides the notification center on the screen named @p screen.
    void toggleCenter(const std::string &screen) noexcept;

    /// Any of the above changed.
    CZSignal<> onChanged;

private:
    void fetchState() noexcept;
    void setState(bool available, bool centerShown, bool dnd, UInt32 count) noexcept;
    std::shared_ptr<CZBus> m_bus;
    sd_bus_slot *m_stateMatch {}, *m_ownerMatch {};
    bool m_available { false }, m_centerShown { false }, m_dnd { false };
    UInt32 m_count { 0 };
};

#endif // NOTIFICATIONS_H
