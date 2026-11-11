#ifndef STATUS_H
#define STATUS_H

#include <Core/Types.h>
#include <Core/CZObject.h>
#include <Core/CZSignal.h>
#include <Core/CZTimer.h>
#include <Core/CZBus.h>
#include <Core/CZEventSource.h>
#include <Core/Worker.h>
#include <memory>
#include <string>
#include <sys/types.h>

struct sd_bus_slot;

using namespace CZ;

/**
 * @brief The system status shown by the bar's indicators: network, volume and battery.
 *
 * Each is exposed as a freedesktop symbolic icon name (empty = nothing to show, e.g. no battery):
 * - Network: NetworkManager (system bus). The Wi-Fi signal strength of the active wireless
 *   connection, otherwise wired, otherwise Wi-Fi offline/disabled. Hidden without NetworkManager
 *   or network devices.
 * - Volume: the default sink of PipeWire/PulseAudio (`wpctl`), refreshed on `pactl subscribe`
 *   events. Muted, low, medium or high.
 * - Battery: UPower's display device (system bus), its level in steps of 10 %, and charging /
 *   charged / plugged in.
 *
 * Everything is event driven (D-Bus signals, the pactl event stream), onChanged is emitted when
 * an icon changes. The queries and wpctl calls run on a Worker thread (never blocking the UI).
 */
class Status final : public CZObject
{
public:
    Status() noexcept;
    ~Status() noexcept;

    std::string networkIcon, volumeIcon, batteryIcon;

    /// Output volume (0 to 1, may exceed 1 if boosted) and mute, -1 without an audio output.
    float volume { -1.f };
    bool muted { false };

    /// Battery level in percent, -1 without a battery.
    Int32 batteryPercent { -1 };

    /// Sets the output volume (0 to 1, unmuting if > 0). Throttled: fine to call on every drag step.
    void setVolume(float volume) noexcept;

    /// Human readable state of each (e.g. "Wi-Fi: 72 %"), for logs.
    std::string networkInfo, volumeInfo, batteryInfo;

    CZSignal<> onChanged;

private:
    void refreshNetwork() noexcept;
    void refreshBattery() noexcept;
    void refreshVolume() noexcept;
    void startVolumeMonitor() noexcept;
    void stopVolumeMonitor() noexcept;
    void applyBattery(double percentage, UInt32 state) noexcept;
    void applyVolumeState(bool ok, float volume, bool muted) noexcept;
    void set(std::string &icon, std::string value) noexcept;
    void applyVolume() noexcept;
    std::unique_ptr<CZTimer> m_setVolumeTimer;
    float m_pendingVolume { -1.f };

    std::shared_ptr<CZBus> m_system;
    sd_bus_slot *m_networkMatch {}, *m_batteryMatch {};

    // Debounce bursts of events (e.g. NetworkManager scans, volume slider drags).
    std::unique_ptr<CZTimer> m_networkTimer, m_volumeTimer, m_volumeRetryTimer;

    pid_t m_pactlPid { -1 };
    std::shared_ptr<CZEventSource> m_pactlSource;
    std::string m_pactlBuffer;

    bool m_networkBusy { false }, m_networkAgain { false };
    bool m_volumeBusy { false }, m_volumeAgain { false };
    bool m_settingVolume { false };

    // Last: destroyed first, so no completion runs on a half destroyed object.
    Worker m_worker;
};

#endif // STATUS_H
