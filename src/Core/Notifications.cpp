#include <Core/Notifications.h>
#include <Core/Log.h>
#include <CZ/Core/CZBus.h>
#include <systemd/sd-bus.h>
#include <cstring>

static constexpr const char *GazetteName { "org.cuarzo.Gazette" };
static constexpr const char *GazettePath { "/org/cuarzo/Gazette" };

Notifications::Notifications() noexcept
{
    m_bus = CZBus::GetOrMakeUser();

    if (!m_bus)
        return;

    sd_bus_match_signal_async(m_bus->bus(), &m_stateMatch, nullptr, GazettePath, GazetteName, "StateChanged",
        [](sd_bus_message *m, void *data, sd_bus_error *) -> int {
            int shown { 0 }, dnd { 0 };
            UInt32 count { 0 };

            if (sd_bus_message_read(m, "bbu", &shown, &dnd, &count) >= 0)
                static_cast<Notifications*>(data)->setState(true, shown, dnd, count);

            return 0;
        }, nullptr, this);

    // Started or gone
    sd_bus_match_signal_async(m_bus->bus(), &m_ownerMatch, "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "NameOwnerChanged",
        [](sd_bus_message *m, void *data, sd_bus_error *) -> int {
            const char *name {}, *oldOwner {}, *newOwner {};

            if (sd_bus_message_read(m, "sss", &name, &oldOwner, &newOwner) < 0 || !name || std::strcmp(name, GazetteName) != 0)
                return 0;

            auto *self { static_cast<Notifications*>(data) };

            if (newOwner && newOwner[0])
                self->fetchState();
            else
                self->setState(false, false, false, 0);

            return 0;
        }, nullptr, this);

    fetchState();
}

Notifications::~Notifications() noexcept
{
    sd_bus_slot_unref(m_stateMatch);
    sd_bus_slot_unref(m_ownerMatch);
}

void Notifications::fetchState() noexcept
{
    if (!m_bus)
        return;

    sd_bus_call_method_async(m_bus->bus(), nullptr, GazetteName, GazettePath, GazetteName, "GetState",
        [](sd_bus_message *m, void *data, sd_bus_error *) -> int {
            auto *self { static_cast<Notifications*>(data) };
            int shown { 0 }, dnd { 0 };
            UInt32 count { 0 };

            // Not running
            if (sd_bus_message_is_method_error(m, nullptr) || sd_bus_message_read(m, "bbu", &shown, &dnd, &count) < 0)
                self->setState(false, false, false, 0);
            else
                self->setState(true, shown, dnd, count);

            return 0;
        }, this, "");
}

void Notifications::setState(bool available, bool centerShown, bool dnd, UInt32 count) noexcept
{
    if (m_available == available && m_centerShown == centerShown && m_dnd == dnd && m_count == count)
        return;

    m_available = available;
    m_centerShown = centerShown;
    m_dnd = dnd;
    m_count = count;
    onChanged.notify();
}

void Notifications::toggleCenter(const std::string &screen) noexcept
{
    if (!m_bus)
        return;

    sd_bus_call_method_async(m_bus->bus(), nullptr, GazetteName, GazettePath, GazetteName, "ToggleCenter",
        nullptr, nullptr, "s", screen.c_str());
}
