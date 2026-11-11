#include <Core/Status.h>
#include <Core/Log.h>
#include <systemd/sd-bus.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

extern char **environ;

static constexpr const char *NM { "org.freedesktop.NetworkManager" };
static constexpr const char *NMPath { "/org/freedesktop/NetworkManager" };
static constexpr const char *UPower { "org.freedesktop.UPower" };
static constexpr const char *UPowerDisplay { "/org/freedesktop/UPower/devices/DisplayDevice" };

/* ---------------------------------- D-Bus property helpers ---------------------------------- */

// Reads a property of a basic type (sd-bus type char @p type) into @p out.
template <class T>
static bool Get(sd_bus *bus, const char *dest, const char *path, const char *iface, const char *prop, char type, T *out) noexcept
{
    return sd_bus_get_property_trivial(bus, dest, path, iface, prop, nullptr, type, out) >= 0;
}

static std::string GetString(sd_bus *bus, const char *dest, const char *path, const char *iface, const char *prop, const char *type = "s") noexcept
{
    sd_bus_message *reply {};
    std::string result;

    if (sd_bus_get_property(bus, dest, path, iface, prop, nullptr, &reply, type) >= 0)
    {
        const char *s {};

        if (sd_bus_message_read(reply, type, &s) >= 0 && s)
            result = s;
    }

    sd_bus_message_unref(reply);
    return result;
}

static std::vector<std::string> GetPaths(sd_bus *bus, const char *dest, const char *path, const char *iface, const char *prop) noexcept
{
    sd_bus_message *reply {};
    std::vector<std::string> result;

    if (sd_bus_get_property(bus, dest, path, iface, prop, nullptr, &reply, "ao") >= 0 &&
        sd_bus_message_enter_container(reply, 'a', "o") >= 0)
    {
        const char *p {};

        while (sd_bus_message_read(reply, "o", &p) > 0)
            result.emplace_back(p);
    }

    sd_bus_message_unref(reply);
    return result;
}

/* ------------------------------------------ Status ------------------------------------------ */

Status::Status() noexcept
{
    m_networkTimer = std::make_unique<CZTimer>([this](CZTimer*){ refreshNetwork(); });
    m_volumeTimer = std::make_unique<CZTimer>([this](CZTimer*){ refreshVolume(); });
    m_volumeRetryTimer = std::make_unique<CZTimer>([this](CZTimer*){ startVolumeMonitor(); });
    m_setVolumeTimer = std::make_unique<CZTimer>([this](CZTimer*){ applyVolume(); });

    m_system = CZBus::GetOrMakeSystem();

    if (m_system)
    {
        // Any NetworkManager property change (connections, devices, access points' strength...).
        sd_bus_match_signal_async(m_system->bus(), &m_networkMatch, NM, nullptr,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            [](sd_bus_message *, void *data, sd_bus_error *) -> int {
                auto *s { static_cast<Status*>(data) };

                if (!s->m_networkTimer->running())
                    s->m_networkTimer->start(500);
                return 0;
            }, nullptr, this);

        sd_bus_match_signal_async(m_system->bus(), &m_batteryMatch, UPower, UPowerDisplay,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            [](sd_bus_message *, void *data, sd_bus_error *) -> int {
                static_cast<Status*>(data)->refreshBattery();
                return 0;
            }, nullptr, this);

        refreshNetwork();
        refreshBattery();
    }
    else
        Log(CZWarning, CZLN, "No system bus: network and battery indicators disabled");

    startVolumeMonitor();
    refreshVolume();
}

Status::~Status() noexcept
{
    stopVolumeMonitor();
    sd_bus_slot_unref(m_networkMatch);
    sd_bus_slot_unref(m_batteryMatch);
}

void Status::set(std::string &icon, std::string value) noexcept
{
    if (icon == value)
        return;

    icon = std::move(value);
    onChanged.notify();
}

void Status::refreshNetwork() noexcept
{
    if (!m_system)
        return;

    // Coalesced: at most one query running, and one more queued.
    if (m_networkBusy)
    {
        m_networkAgain = true;
        return;
    }

    m_networkBusy = true;

    // Many calls: read off the UI thread.
    m_worker.run([this](sd_bus *bus) -> std::function<void()> {
        std::string icon, info;

        if (bus)
        {
            int wifiStrength { -1 };
            bool wired { false };

            // Active connections: a wireless one gives the signal of its access point.
            for (const auto &active : GetPaths(bus, NM, NMPath, NM, "ActiveConnections"))
            {
                const char *iface { "org.freedesktop.NetworkManager.Connection.Active" };
                UInt32 state { 0 };

                if (!Get(bus, NM, active.c_str(), iface, "State", 'u', &state) || state != 2 /* Activated */)
                    continue;

                const std::string type { GetString(bus, NM, active.c_str(), iface, "Type") };

                if (type == "802-3-ethernet")
                    wired = true;
                else if (type == "802-11-wireless")
                {
                    for (const auto &device : GetPaths(bus, NM, active.c_str(), iface, "Devices"))
                    {
                        const std::string ap { GetString(bus, NM, device.c_str(), "org.freedesktop.NetworkManager.Device.Wireless", "ActiveAccessPoint", "o") };
                        UInt8 strength { 0 };

                        if (ap.size() > 1 && Get(bus, NM, ap.c_str(), "org.freedesktop.NetworkManager.AccessPoint", "Strength", 'y', &strength))
                            wifiStrength = std::max(wifiStrength, Int32(strength));
                    }
                }
            }

            // No active Wi-Fi: is there a Wi-Fi device at all?
            bool hasWifi { wifiStrength >= 0 };

            if (!hasWifi)
                for (const auto &device : GetPaths(bus, NM, NMPath, NM, "Devices"))
                {
                    UInt32 type { 0 };

                    if (Get(bus, NM, device.c_str(), "org.freedesktop.NetworkManager.Device", "DeviceType", 'u', &type) && type == 2 /* Wi-Fi */)
                    {
                        hasWifi = true;
                        break;
                    }
                }

            if (wifiStrength >= 0)
            {
                const char *level { wifiStrength >= 80 ? "excellent" : wifiStrength >= 55 ? "good" :
                                    wifiStrength >= 30 ? "ok" : wifiStrength > 5 ? "weak" : "none" };
                icon = std::string("network-wireless-signal-") + level + "-symbolic";
                info = "Wi-Fi " + std::to_string(wifiStrength) + " %";
            }
            else if (wired)
            {
                icon = "network-wired-symbolic";
                info = "Wired";
            }
            else if (hasWifi)
            {
                int enabled { 1 };
                Get(bus, NM, NMPath, NM, "WirelessEnabled", 'b', &enabled);
                icon = enabled ? "network-wireless-offline-symbolic" : "network-wireless-disabled-symbolic";
                info = enabled ? "Wi-Fi disconnected" : "Wi-Fi off";
            }
            else
                info = "No network devices";
        }

        return [this, icon, info]{
            m_networkBusy = false;
            networkInfo = info;
            set(networkIcon, icon);

            if (m_networkAgain)
            {
                m_networkAgain = false;
                refreshNetwork();
            }
        };
    });
}

void Status::refreshBattery() noexcept
{
    if (!m_system)
        return;

    m_worker.run([this](sd_bus *bus) -> std::function<void()> {
        const char *iface { "org.freedesktop.UPower.Device" };
        int present { 0 };
        double percentage { 0.0 };
        UInt32 state { 0 };

        if (!bus ||
            !Get(bus, UPower, UPowerDisplay, iface, "IsPresent", 'b', &present) || !present ||
            !Get(bus, UPower, UPowerDisplay, iface, "Percentage", 'd', &percentage))
        {
            return [this]{
                batteryInfo = "No battery";

                if (batteryPercent != -1)
                {
                    batteryPercent = -1;
                    onChanged.notify();
                }

                set(batteryIcon, "");
            };
        }

        Get(bus, UPower, UPowerDisplay, iface, "State", 'u', &state);

        return [this, percentage, state]{ applyBattery(percentage, state); };
    });
}

void Status::applyBattery(double percentage, UInt32 state) noexcept
{
    // battery-level-{0..100}[-charging|-charged|-plugged-in]-symbolic
    const Int32 level { std::clamp(Int32(std::lround(percentage / 10.0)) * 10, 0, 100) };
    std::string suffix;

    if (state == 1)                         suffix = "-charging";      // Charging
    else if (state == 4 && level == 100)    suffix = "-charged";       // Fully charged
    else if (state == 4 || state == 5)      suffix = "-plugged-in";    // On AC, not charging

    batteryInfo = "Battery " + std::to_string(Int32(std::lround(percentage))) + " %" + suffix;

    if (const Int32 pct { Int32(std::lround(percentage)) }; pct != batteryPercent)
    {
        batteryPercent = pct;
        onChanged.notify();
    }

    set(batteryIcon, "battery-level-" + std::to_string(level) + suffix + "-symbolic");
}

/* ------------------------------------------ Volume ------------------------------------------ */

void Status::refreshVolume() noexcept
{
    // Coalesced: at most one wpctl running, and one more queued.
    if (m_volumeBusy)
    {
        m_volumeAgain = true;
        return;
    }

    m_volumeBusy = true;

    m_worker.run([this](sd_bus *) -> std::function<void()> {
        // "Volume: 0.45" or "Volume: 0.45 [MUTED]"
        FILE *f { popen("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", "r") };
        char line[128] {};
        bool ok { false };
        float volume { 0.f };
        bool muted { false };

        if (f)
        {
            if (fgets(line, sizeof(line), f) && sscanf(line, "Volume: %f", &volume) == 1)
            {
                ok = true;
                muted = strstr(line, "[MUTED]") != nullptr;
            }

            pclose(f);
        }

        if (!ok)
            volume = -1.f;

        return [this, ok, volume, muted]{
            m_volumeBusy = false;
            applyVolumeState(ok, volume, muted);

            if (m_volumeAgain)
            {
                m_volumeAgain = false;
                refreshVolume();
            }
        };
    });
}

void Status::applyVolumeState(bool ok, float volume, bool muted) noexcept
{
    if (std::abs(volume - this->volume) > 0.001f || muted != this->muted)
    {
        this->volume = volume;
        this->muted = muted;
        onChanged.notify();
    }

    if (!ok)
    {
        volumeInfo = "No audio output";
        set(volumeIcon, "");
        return;
    }

    const char *level { muted || volume <= 0.f ? "muted" : volume < 0.34f ? "low" : volume < 0.67f ? "medium" : "high" };
    volumeInfo = "Volume " + std::to_string(Int32(std::lround(volume * 100.f))) + " %" + (muted ? " (muted)" : "");
    set(volumeIcon, std::string("audio-volume-") + level + "-symbolic");
}

void Status::startVolumeMonitor() noexcept
{
    stopVolumeMonitor();

    // `pactl subscribe` prints a line per server event, e.g. "Event 'change' on sink #56" (the
    // default sink changing is a "server" event).
    int fds[2];

    if (pipe2(fds, O_CLOEXEC) != 0)
        return;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    // Untranslated output ("Event 'change' on sink #56", parsed below), whatever the user's locale.
    char *argv[] { const_cast<char*>("env"), const_cast<char*>("LC_ALL=C"), const_cast<char*>("pactl"), const_cast<char*>("subscribe"), nullptr };
    const int ret { posix_spawnp(&m_pactlPid, "env", &actions, nullptr, argv, environ) };
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);

    if (ret != 0)
    {
        m_pactlPid = -1;
        close(fds[0]);
        Log(CZWarning, CZLN, "Failed to run pactl subscribe: volume changes won't be tracked");
        return;
    }

    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);

    m_pactlSource = CZEventSource::Make(fds[0], EPOLLIN, CZOwn::Own, [this](int fd, UInt32 events, CZEventSource *source) {
        char buf[1024];
        ssize_t n;
        bool changed { false };

        while ((n = read(fd, buf, sizeof(buf))) > 0)
            m_pactlBuffer.append(buf, size_t(n));

        size_t nl;

        while ((nl = m_pactlBuffer.find('\n')) != std::string::npos)
        {
            const std::string line { m_pactlBuffer.substr(0, nl) };
            m_pactlBuffer.erase(0, nl + 1);

            if (line.find(" sink ") != std::string::npos || line.find(" server") != std::string::npos)
                changed = true;
        }

        if (changed && !m_volumeTimer->running())
            m_volumeTimer->start(30);

        // pactl exited (e.g. the audio server restarted): try again in a while.
        if (n == 0 || (events & (EPOLLHUP | EPOLLERR)))
        {
            source->setEvents(0); // Stop polling the dead pipe until restarted
            m_volumeRetryTimer->start(3000);
            m_volumeTimer->start(30);
        }
    });
}

void Status::stopVolumeMonitor() noexcept
{
    m_pactlSource.reset();
    m_pactlBuffer.clear();

    if (m_pactlPid > 0)
    {
        kill(m_pactlPid, SIGTERM);
        waitpid(m_pactlPid, nullptr, 0);
        m_pactlPid = -1;
    }
}

void Status::setVolume(float value) noexcept
{
    m_pendingVolume = std::clamp(value, 0.f, 1.f);

    // At most every 40 ms: each step runs wpctl.
    if (!m_setVolumeTimer->running())
        m_setVolumeTimer->start(40);
}

void Status::applyVolume() noexcept
{
    // Still setting the previous one: this one follows (see below).
    if (m_pendingVolume < 0.f || m_settingVolume)
        return;

    char value[16];
    snprintf(value, sizeof(value), "%.3f", m_pendingVolume);
    const bool unmute { m_pendingVolume > 0.f && muted };
    m_pendingVolume = -1.f;

    static const auto run = [](std::vector<const char*> args) {
        args.push_back(nullptr);
        pid_t pid;
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

        if (posix_spawnp(&pid, args[0], &actions, nullptr, const_cast<char**>(args.data()), environ) == 0)
            waitpid(pid, nullptr, 0); // wpctl returns right away

        posix_spawn_file_actions_destroy(&actions);
    };

    // Off the UI thread, in order. The next pending value (dragging) once this one is set.
    m_settingVolume = true;
    m_worker.run([this, value = std::string(value), unmute](sd_bus *) -> std::function<void()> {
        run({ "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", value.c_str() });

        if (unmute)
            run({ "wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "0" });

        return [this]{
            m_settingVolume = false;

            if (m_pendingVolume >= 0.f && !m_setVolumeTimer->running())
                applyVolume();
        };
    });

    // pactl's change event refreshes the state (and the indicators) right after.
}
