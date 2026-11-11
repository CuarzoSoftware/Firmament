#include <Core/Controls.h>
#include <Core/Log.h>
#include <Marco/MApp.h>
#include <systemd/sd-bus.h>
#include <linux/rfkill.h>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <sys/wait.h>

extern char **environ;

static constexpr const char *NM { "org.freedesktop.NetworkManager" };
static constexpr const char *NMPath { "/org/freedesktop/NetworkManager" };
static constexpr const char *BlueZ { "org.bluez" };
static constexpr const char *Login1 { "org.freedesktop.login1" };
static constexpr const char *SessionPath { "/org/freedesktop/login1/session/auto" };

/* ------------------------------------------ Helpers ------------------------------------------ */

static UInt32 ReadUInt(const std::filesystem::path &path) noexcept
{
    std::ifstream f { path };
    UInt32 v { 0 };
    f >> v;
    return v;
}

// Sets a boolean D-Bus property without waiting for the reply.
static void SetBoolProperty(sd_bus *bus, const char *dest, const char *path, const char *iface, const char *prop, bool value) noexcept
{
    sd_bus_call_method_async(bus, nullptr, dest, path, "org.freedesktop.DBus.Properties", "Set",
        [](sd_bus_message *m, void *, sd_bus_error *) -> int {
            if (sd_bus_message_is_method_error(m, nullptr))
                Log(CZWarning, CZLN, "Failed to set property: {}", sd_bus_message_get_error(m)->message);
            return 0;
        }, nullptr, "ssv", iface, prop, "b", int(value));
}

static bool InPath(const char *program) noexcept
{
    const char *path { getenv("PATH") };

    if (!path)
        return false;

    std::string p { path };
    size_t start { 0 };

    while (start <= p.size())
    {
        const size_t end { std::min(p.find(':', start), p.size()) };

        if (access((p.substr(start, end - start) + "/" + program).c_str(), X_OK) == 0)
            return true;

        start = end + 1;
    }

    return false;
}

// Where the Night Shift state is remembered.
static std::filesystem::path NightLightFile() noexcept
{
    const char *config { getenv("XDG_CONFIG_HOME") };
    const char *home { getenv("HOME") };
    const std::filesystem::path dir { config && *config ? std::filesystem::path(config) :
                                      std::filesystem::path(home ? home : "/tmp") / ".config" };
    return dir / "cz-firmament" / "night-light";
}

/* ------------------------------------------ Controls ------------------------------------------ */

Controls::Controls() noexcept
{
    m_brightnessTimer = std::make_unique<CZTimer>([this](CZTimer*){ applyBrightness(); });
    m_bluetoothRefresh = std::make_unique<CZTimer>([this](CZTimer*){ refreshBluetooth(); });
    m_bluetoothRetry = std::make_unique<CZTimer>([this](CZTimer*){
        if (!m_bluezAdapter.empty())
            SetBoolProperty(m_system->bus(), BlueZ, m_bluezAdapter.c_str(), "org.bluez.Adapter1", "Powered", true);
    });

    // Lights (sysfs): the first backlight and keyboard backlight LED.
    std::error_code ec;

    for (const auto &dir : std::filesystem::directory_iterator("/sys/class/backlight", ec))
    {
        m_backlight = dir.path().filename();
        m_backlightMax = ReadUInt(dir.path() / "max_brightness");
        break;
    }

    for (const auto &dir : std::filesystem::directory_iterator("/sys/class/leds", ec))
    {
        if (dir.path().filename().string().find("kbd_backlight") != std::string::npos)
        {
            m_keyboardLed = dir.path().filename();
            m_keyboardMax = ReadUInt(dir.path() / "max_brightness");
            break;
        }
    }

    initRfkill();

    m_system = CZBus::GetOrMakeSystem();

    if (m_system)
    {
        sd_bus_match_signal_async(m_system->bus(), &m_nmMatch, NM, NMPath, "org.freedesktop.DBus.Properties", "PropertiesChanged",
            [](sd_bus_message *, void *data, sd_bus_error *) -> int {
                static_cast<Controls*>(data)->refreshWifi();
                return 0;
            }, nullptr, this);

        sd_bus_match_signal_async(m_system->bus(), &m_bluezMatch, BlueZ, nullptr, "org.freedesktop.DBus.Properties", "PropertiesChanged",
            [](sd_bus_message *, void *data, sd_bus_error *) -> int {
                // Frequent while discovering (devices' signal strength): debounced.
                auto *c { static_cast<Controls*>(data) };

                if (!c->m_bluetoothRefresh->running())
                    c->m_bluetoothRefresh->start(300);
                return 0;
            }, nullptr, this);
    }

    // Night Shift: on again if it was when the session ended.
    nightLightAvailable = InPath("gammastep");

    if (nightLightAvailable)
    {
        std::ifstream f { NightLightFile() };
        std::string saved;
        f >> saved;

        if (saved == "on")
            setNightLight(true);
    }

    refresh();
}

Controls::~Controls() noexcept
{
    stopNightLight(); // Restores the colors
    sd_bus_slot_unref(m_nmMatch);
    sd_bus_slot_unref(m_bluezMatch);
}

void Controls::notify() noexcept
{
    onChanged.notify();
}

void Controls::refresh() noexcept
{
    refreshWifi();
    refreshBluetooth();
    refreshLights();
}

/* --------------------------------------- Radios (rfkill) --------------------------------------- */

void Controls::initRfkill() noexcept
{
    const int fd { open("/dev/rfkill", O_RDWR | O_NONBLOCK | O_CLOEXEC) };

    if (fd < 0)
    {
        Log(CZWarning, CZLN, "Can't open /dev/rfkill: airplane mode unavailable");
        return;
    }

    // Reading it lists every radio (an ADD event each), then reports changes.
    m_rfkill = CZEventSource::Make(fd, EPOLLIN, CZOwn::Own, [this](int, UInt32, CZEventSource*) { readRfkill(); });
    readRfkill();
}

void Controls::readRfkill() noexcept
{
    if (!m_rfkill)
        return;

    rfkill_event e;
    bool changed { false };

    while (read(m_rfkill->fd(), &e, RFKILL_EVENT_SIZE_V1) == RFKILL_EVENT_SIZE_V1)
    {
        changed = true;

        if (e.op == RFKILL_OP_DEL)
            m_radios.erase(e.idx);
        else if (e.op == RFKILL_OP_ADD || e.op == RFKILL_OP_CHANGE)
            m_radios[e.idx] = { e.type, e.soft != 0, e.hard != 0 };
    }

    if (!changed)
        return;

    airplaneAvailable = !m_radios.empty();
    airplaneMode = airplaneAvailable && std::all_of(m_radios.begin(), m_radios.end(),
        [](const auto &r){ return r.second.soft || r.second.hard; });

    wifiAvailable = std::any_of(m_radios.begin(), m_radios.end(), [](const auto &r){ return r.second.type == RFKILL_TYPE_WLAN; });

    // The adapter may come and go with its radio.
    refreshWifi();
    refreshBluetooth();
    notify();
}

void Controls::rfkillBlock(UInt8 type, bool block) noexcept
{
    if (!m_rfkill)
        return;

    rfkill_event e {};
    e.op = RFKILL_OP_CHANGE_ALL;
    e.type = type;
    e.soft = block;

    if (write(m_rfkill->fd(), &e, RFKILL_EVENT_SIZE_V1) != RFKILL_EVENT_SIZE_V1)
        Log(CZWarning, CZLN, "Failed to change the radios' block state: {}", strerror(errno));
}

void Controls::setAirplaneMode(bool enabled) noexcept
{
    rfkillBlock(RFKILL_TYPE_ALL, enabled);
}

/* ------------------------------------------- Wi-Fi ------------------------------------------- */

void Controls::refreshWifi() noexcept
{
    if (!m_system)
        return;

    // Read off the UI thread (NetworkManager may be slow to answer).
    m_worker.run([this](sd_bus *bus) -> std::function<void()> {
        int enabled { 0 };

        if (!bus || sd_bus_get_property_trivial(bus, NM, NMPath, NM, "WirelessEnabled", nullptr, 'b', &enabled) < 0)
            return {};

        return [this, enabled]{
            // Also off while its radio is blocked.
            const bool blocked { std::any_of(m_radios.begin(), m_radios.end(),
                [](const auto &r){ return r.second.type == RFKILL_TYPE_WLAN && (r.second.soft || r.second.hard); }) };

            const bool on { enabled && !blocked };

            if (on != wifiEnabled)
            {
                wifiEnabled = on;
                notify();
            }
        };
    });
}

void Controls::setWifi(bool enabled) noexcept
{
    if (enabled)
        rfkillBlock(RFKILL_TYPE_WLAN, false);

    if (m_system)
        SetBoolProperty(m_system->bus(), NM, NMPath, NM, "WirelessEnabled", enabled);
}

/* ----------------------------------------- Bluetooth ----------------------------------------- */

void Controls::refreshBluetooth() noexcept
{
    if (!m_system)
        return;

    // Coalesced: at most one probe running, and one more queued.
    if (m_bluetoothBusy)
    {
        m_bluetoothAgain = true;
        return;
    }

    m_bluetoothBusy = true;

    m_worker.run([this](sd_bus *bus) -> std::function<void()> {
        // The first adapter.
        std::string adapter;
        int powered { 0 };

        for (int i = 0; bus && i < 4 && adapter.empty(); i++)
        {
            const std::string path { "/org/bluez/hci" + std::to_string(i) };

            if (sd_bus_get_property_trivial(bus, BlueZ, path.c_str(), "org.bluez.Adapter1", "Powered", nullptr, 'b', &powered) >= 0)
                adapter = path;
        }

        return [this, adapter, powered]{
            m_bluetoothBusy = false;

            const bool hasRadio { std::any_of(m_radios.begin(), m_radios.end(), [](const auto &r){ return r.second.type == RFKILL_TYPE_BLUETOOTH; }) };
            const bool available { !adapter.empty() || hasRadio };
            const bool on { !adapter.empty() && powered };
            m_bluezAdapter = adapter;

            if (available != bluetoothAvailable || on != bluetoothEnabled)
            {
                bluetoothAvailable = available;
                bluetoothEnabled = on;
                notify();
            }

            if (m_bluetoothAgain)
            {
                m_bluetoothAgain = false;
                refreshBluetooth();
            }
        };
    });
}

void Controls::setBluetooth(bool enabled) noexcept
{
    if (!m_system)
        return;

    if (enabled)
    {
        // A blocked radio hides the adapter: unblock it, then power it on once it's back.
        const bool blocked { std::any_of(m_radios.begin(), m_radios.end(),
            [](const auto &r){ return r.second.type == RFKILL_TYPE_BLUETOOTH && r.second.soft; }) };

        if (blocked || m_bluezAdapter.empty())
        {
            rfkillBlock(RFKILL_TYPE_BLUETOOTH, false);
            m_bluetoothRetry->start(800);
            return;
        }
    }

    if (!m_bluezAdapter.empty())
        SetBoolProperty(m_system->bus(), BlueZ, m_bluezAdapter.c_str(), "org.bluez.Adapter1", "Powered", enabled);
}

/* ------------------------------------------ Lights ------------------------------------------ */

void Controls::refreshLights() noexcept
{
    bool changed { false };

    if (!m_backlight.empty() && m_backlightMax > 0 && m_pendingBrightness < 0.f)
    {
        const SkScalar b { SkScalar(ReadUInt("/sys/class/backlight/" + m_backlight + "/brightness")) / SkScalar(m_backlightMax) };
        changed |= std::abs(b - brightness) > 0.001f;
        brightness = b;
    }

    if (!m_keyboardLed.empty() && m_keyboardMax > 0)
    {
        const UInt32 level { ReadUInt("/sys/class/leds/" + m_keyboardLed + "/brightness") };

        if (level > 0)
            m_keyboardLast = level;

        changed |= !keyboardLightAvailable || keyboardLightOn != (level > 0);
        keyboardLightAvailable = true;
        keyboardLightOn = level > 0;
    }

    if (changed)
        notify();
}

void Controls::setLogindBrightness(const char *subsystem, const std::string &name, UInt32 value) noexcept
{
    if (!m_system)
        return;

    sd_bus_call_method_async(m_system->bus(), nullptr, Login1, SessionPath, "org.freedesktop.login1.Session", "SetBrightness",
        [](sd_bus_message *m, void *, sd_bus_error *) -> int {
            if (sd_bus_message_is_method_error(m, nullptr))
                Log(CZWarning, CZLN, "SetBrightness failed: {}", sd_bus_message_get_error(m)->message);
            return 0;
        }, nullptr, "ssu", subsystem, name.c_str(), value);
}

void Controls::setBrightness(SkScalar value) noexcept
{
    if (m_backlight.empty())
        return;

    // Never fully dark.
    m_pendingBrightness = std::clamp(value, 0.02f, 1.f);
    brightness = m_pendingBrightness;

    if (!m_brightnessTimer->running())
        m_brightnessTimer->start(30);
}

void Controls::applyBrightness() noexcept
{
    if (m_pendingBrightness < 0.f)
        return;

    setLogindBrightness("backlight", m_backlight, UInt32(std::lround(m_pendingBrightness * m_backlightMax)));
    m_pendingBrightness = -1.f;
}

void Controls::setKeyboardLight(bool on) noexcept
{
    if (m_keyboardLed.empty())
        return;

    // Back to the last level used (e.g. set with the keyboard's own key).
    const UInt32 level { on ? (m_keyboardLast > 0 ? m_keyboardLast : m_keyboardMax) : 0 };
    setLogindBrightness("leds", m_keyboardLed, level);
    keyboardLightOn = on;
    notify();
}

/* ------------------------------------------ Appearance ------------------------------------------ */

bool Controls::darkMode() const noexcept
{
    auto app { MApp::Get() };
    return app && app->colorScheme() == CZColorScheme::Dark;
}

void Controls::setDarkMode(bool dark) noexcept
{
    // Right away here, then system wide: the portal notifies every app (this one included).
    if (auto app { MApp::Get() })
        app->setColorScheme(dark ? CZColorScheme::Dark : CZColorScheme::Light);

    SpawnDetached({ "gsettings", "set", "org.gnome.desktop.interface", "color-scheme", dark ? "prefer-dark" : "default" });
}

/* ----------------------------------------- Night Shift ----------------------------------------- */

static constexpr const char *NightLightTemperature { "4000" }; // K (6500 = neutral)

void Controls::setNightLight(bool on) noexcept
{
    if (!nightLightAvailable)
        return;

    // Remembered for the next session.
    {
        std::error_code ec;
        std::filesystem::create_directories(NightLightFile().parent_path(), ec);
        std::ofstream f { NightLightFile() };
        f << (on ? "on" : "off") << '\n';
    }

    if (!on)
    {
        stopNightLight();
        nightLightOn = false;
        notify();
        return;
    }

    if (m_nightLightPid > 0)
        return;

    // One-shot manual mode: on Wayland it keeps running (the compositor resets the gamma when it
    // exits). -P: from the default gamma, -r: no transitions (it stops right away).
    char *argv[] {
        const_cast<char*>("gammastep"), const_cast<char*>("-m"), const_cast<char*>("wayland"),
        const_cast<char*>("-P"), const_cast<char*>("-r"), const_cast<char*>("-O"), const_cast<char*>(NightLightTemperature), nullptr };

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid;
    const int ret { posix_spawnp(&pid, "gammastep", &actions, nullptr, argv, environ) };
    posix_spawn_file_actions_destroy(&actions);

    if (ret != 0)
    {
        Log(CZWarning, CZLN, "Failed to run gammastep: {}", strerror(ret));
        notify();
        return;
    }

    m_nightLightPid = pid;
    nightLightOn = true;

    // Notified if it exits by itself (e.g. no gamma control support, or another app has it).
    const int pidfd { int(syscall(SYS_pidfd_open, pid, 0)) };

    if (pidfd >= 0)
        m_nightLightWatch = CZEventSource::Make(pidfd, EPOLLIN, CZOwn::Own, [this](int, UInt32, CZEventSource*) {
            nightLightExited();
        });

    notify();
}

void Controls::nightLightExited() noexcept
{
    m_nightLightWatch.reset();

    if (m_nightLightPid > 0)
    {
        waitpid(m_nightLightPid, nullptr, WNOHANG);
        m_nightLightPid = -1;
        Log(CZWarning, CZLN, "gammastep exited: Night Shift is off");
    }

    nightLightOn = false;
    notify();
}

void Controls::stopNightLight() noexcept
{
    m_nightLightWatch.reset();

    if (m_nightLightPid <= 0)
        return;

    // It exits right away on SIGTERM (restoring the colors).
    kill(m_nightLightPid, SIGTERM);
    waitpid(m_nightLightPid, nullptr, 0);
    m_nightLightPid = -1;
}

/* ------------------------------------------- Session ------------------------------------------- */

void Controls::logindCall(const char *method, bool session) noexcept
{
    if (!m_system)
        return;

    const auto reply = [](sd_bus_message *m, void *, sd_bus_error *) -> int {
        if (sd_bus_message_is_method_error(m, nullptr))
            Log(CZWarning, CZLN, "logind call failed: {}", sd_bus_message_get_error(m)->message);
        return 0;
    };

    if (session)
        sd_bus_call_method_async(m_system->bus(), nullptr, Login1, SessionPath, "org.freedesktop.login1.Session", method, reply, nullptr, "");
    else // Interactive: polkit may ask for authorization
        sd_bus_call_method_async(m_system->bus(), nullptr, Login1, "/org/freedesktop/login1", "org.freedesktop.login1.Manager", method, reply, nullptr, "b", 1);
}

void Controls::lock() noexcept
{
    // A session-lock client; otherwise ask logind (whatever locker listens for it).
    if (InPath("swaylock"))
        SpawnDetached({ "swaylock", "-f", "-c", "000000" });
    else
        logindCall("Lock", true);
}

void Controls::openSettings() noexcept
{
    // GNOME Settings refuses to run outside GNOME without this.
    if (InPath("gnome-control-center"))
        SpawnDetached({ "gnome-control-center" }, { "XDG_CURRENT_DESKTOP=GNOME" });
    else if (InPath("systemsettings"))
        SpawnDetached({ "systemsettings" });
    else
        Log(CZWarning, CZLN, "No system settings app found (gnome-control-center, systemsettings)");
}

void Controls::suspend() noexcept  { logindCall("Suspend", false); }
void Controls::restart() noexcept  { logindCall("Reboot", false); }
void Controls::powerOff() noexcept { logindCall("PowerOff", false); }
void Controls::logOut() noexcept   { logindCall("Terminate", true); }
