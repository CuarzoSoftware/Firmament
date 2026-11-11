#ifndef CONTROLS_H
#define CONTROLS_H

#include <Core/Types.h>
#include <Core/CZObject.h>
#include <Core/CZSignal.h>
#include <Core/CZTimer.h>
#include <Core/CZBus.h>
#include <Core/CZEventSource.h>
#include <Core/Worker.h>
#include <map>
#include <sys/types.h>
#include <memory>
#include <string>

struct sd_bus_slot;

using namespace CZ;

/**
 * @brief The system controls behind the control panel: radios, lights, appearance and session.
 *
 * - Wi-Fi: NetworkManager's WirelessEnabled.
 * - Bluetooth: the BlueZ adapter's Powered (unblocking its radio first if needed).
 * - Airplane mode: blocks/unblocks every radio (/dev/rfkill). On while all of them are blocked.
 * - Display brightness and keyboard light: sysfs backlight/LEDs, set through logind
 *   (Session.SetBrightness, no root needed).
 * - Night Shift: warmer colors, by running gammastep at a fixed temperature (wlr gamma control,
 *   reset as soon as it exits). Remembered across sessions.
 * - Dark mode: the desktop's color-scheme preference (GNOME's setting, read by the XDG portal, so
 *   Marco, GTK, Qt and Chromium apps follow it).
 * - Session: lock (swaylock, or logind), suspend, restart, log out and power off (logind), and the
 *   system settings (GNOME Settings).
 *
 * States are kept in sync from D-Bus signals and rfkill events (sysfs values on refresh()), and
 * onChanged is emitted on changes. Nothing blocks the UI: D-Bus reads run on a Worker thread,
 * D-Bus writes and logind calls are async, and commands are spawned detached.
 */
class Controls final : public CZObject
{
public:
    Controls() noexcept;
    ~Controls() noexcept;

    bool wifiAvailable { false }, wifiEnabled { false };
    bool bluetoothAvailable { false }, bluetoothEnabled { false };
    bool airplaneAvailable { false }, airplaneMode { false };
    bool keyboardLightAvailable { false }, keyboardLightOn { false };
    bool nightLightAvailable { false }, nightLightOn { false };
    SkScalar brightness { -1.f }; ///< 0 to 1, -1 without a backlight

    void setWifi(bool enabled) noexcept;
    void setBluetooth(bool enabled) noexcept;
    void setAirplaneMode(bool enabled) noexcept;
    void setKeyboardLight(bool on) noexcept;
    void setBrightness(SkScalar value) noexcept; ///< Throttled: fine on every drag step
    void setDarkMode(bool dark) noexcept;
    void setNightLight(bool on) noexcept;
    bool darkMode() const noexcept;

    void lock() noexcept;
    void openSettings() noexcept;
    void suspend() noexcept;
    void restart() noexcept;
    void logOut() noexcept;
    void powerOff() noexcept;

    /// Re-reads the states not notified by events (brightness, keyboard light).
    void refresh() noexcept;

    CZSignal<> onChanged;

private:
    struct Radio { UInt8 type; bool soft, hard; };

    void initRfkill() noexcept;
    void readRfkill() noexcept;
    void rfkillBlock(UInt8 type, bool block) noexcept;
    void refreshWifi() noexcept;
    void refreshBluetooth() noexcept;
    void refreshLights() noexcept;
    void applyBrightness() noexcept;
    void setLogindBrightness(const char *subsystem, const std::string &name, UInt32 value) noexcept;
    void logindCall(const char *method, bool session) noexcept;
    void notify() noexcept;
    void stopNightLight() noexcept;
    void nightLightExited() noexcept;

    pid_t m_nightLightPid { -1 };
    std::shared_ptr<CZEventSource> m_nightLightWatch; // pidfd: gammastep exited

    std::shared_ptr<CZBus> m_system;
    sd_bus_slot *m_nmMatch {}, *m_bluezMatch {};
    std::string m_bluezAdapter;

    std::shared_ptr<CZEventSource> m_rfkill;
    std::map<UInt32, Radio> m_radios;

    std::string m_backlight, m_keyboardLed; // sysfs names
    UInt32 m_backlightMax { 0 }, m_keyboardMax { 0 }, m_keyboardLast { 0 };
    SkScalar m_pendingBrightness { -1.f };
    std::unique_ptr<CZTimer> m_brightnessTimer, m_bluetoothRetry, m_bluetoothRefresh;
    bool m_bluetoothBusy { false }, m_bluetoothAgain { false };

    // Last: destroyed first, so no completion runs on a half destroyed object.
    Worker m_worker;
};

#endif // CONTROLS_H
