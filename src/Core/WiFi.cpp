#include <Core/WiFi.h>
#include <CZ/Core/CZBus.h>
#include <CZ/Core/CZTime.h>
#include <CZ/Core/CZLog.h>
#include <systemd/sd-bus.h>
#include <spawn.h>
#include <sys/wait.h>
#include <algorithm>
#include <unordered_map>
#include <variant>

extern char **environ;

using namespace CZ;

static constexpr const char *NM { "org.freedesktop.NetworkManager" };
static constexpr const char *NMPath { "/org/freedesktop/NetworkManager" };
static constexpr const char *NMDevice { "org.freedesktop.NetworkManager.Device" };
static constexpr const char *NMWireless { "org.freedesktop.NetworkManager.Device.Wireless" };
static constexpr const char *NMAccessPoint { "org.freedesktop.NetworkManager.AccessPoint" };
static constexpr const char *NMSettingsPath { "/org/freedesktop/NetworkManager/Settings" };
static constexpr const char *NMSettings { "org.freedesktop.NetworkManager.Settings" };
static constexpr const char *NMConnection { "org.freedesktop.NetworkManager.Settings.Connection" };

// NM_802_11_AP_FLAGS / NM_802_11_AP_SEC
static constexpr UInt32 APPrivacy { 0x1 };
static constexpr UInt32 KeyMgmtPSK { 0x100 }, KeyMgmt8021X { 0x200 }, KeyMgmtSAE { 0x400 };

// NMDeviceState
static constexpr UInt32 DeviceDisconnected { 30 }, DevicePrepare { 40 }, DeviceActivated { 100 }, DeviceFailed { 120 };

/* ----------------------------------- D-Bus value parsing ----------------------------------- */

namespace
{
    using Value = std::variant<std::monostate, bool, UInt64, std::string, std::vector<UInt8>, std::vector<std::string>, std::vector<UInt64>>;
    using Dict = std::unordered_map<std::string, Value>;
    using Settings = std::unordered_map<std::string, Dict>;

    template <class T>
    const T *Get(const Dict &d, const char *key) noexcept
    {
        const auto it { d.find(key) };
        return it == d.end() ? nullptr : std::get_if<T>(&it->second);
    }

    template <class T>
    T GetOr(const Dict &d, const char *key, T fallback) noexcept
    {
        const T *v { Get<T>(d, key) };
        return v ? *v : fallback;
    }

    bool ReadDict(sd_bus_message *m, Dict &out) noexcept;

    // At a variant: its value (the types used here, others skipped)
    void ReadVariant(sd_bus_message *m, Value &out) noexcept
    {
        char type;
        const char *contents {};

        if (sd_bus_message_peek_type(m, &type, &contents) <= 0 || type != 'v' || !contents ||
            sd_bus_message_enter_container(m, 'v', contents) < 0)
        {
            sd_bus_message_skip(m, nullptr);
            return;
        }

        const std::string sig { contents };

        if (sig == "b")
        {
            int b {};
            sd_bus_message_read(m, "b", &b);
            out = bool(b);
        }
        else if (sig == "u" || sig == "y" || sig == "i")
        {
            if (sig == "y") { uint8_t v {}; sd_bus_message_read(m, "y", &v); out = UInt64(v); }
            else if (sig == "u") { uint32_t v {}; sd_bus_message_read(m, "u", &v); out = UInt64(v); }
            else { int32_t v {}; sd_bus_message_read(m, "i", &v); out = UInt64(UInt32(v)); }
        }
        else if (sig == "s" || sig == "o")
        {
            const char *s {};
            sd_bus_message_read(m, sig.c_str(), &s);
            out = std::string(s ? s : "");
        }
        else if (sig == "ay")
        {
            const void *p {};
            size_t n {};
            sd_bus_message_read_array(m, 'y', &p, &n);
            const auto *b { static_cast<const UInt8*>(p) };
            out = std::vector<UInt8>(b, b + n);
        }
        else if (sig == "ao" || sig == "as")
        {
            std::vector<std::string> list;

            if (sd_bus_message_enter_container(m, 'a', sig.c_str() + 1) >= 0)
            {
                const char *s {};

                while (sd_bus_message_read(m, sig.c_str() + 1, &s) > 0)
                    list.emplace_back(s);

                sd_bus_message_exit_container(m);
            }

            out = std::move(list);
        }
        else if (sig == "(uu)")
        {
            // StateReason: (state, reason)
            uint32_t a {}, b {};
            sd_bus_message_read(m, "(uu)", &a, &b);
            out = std::vector<UInt64> { a, b };
        }
        else if (sig == "aa{sv}")
        {
            // AddressData: the first address
            if (sd_bus_message_enter_container(m, 'a', "a{sv}") >= 0)
            {
                Dict first;

                if (ReadDict(m, first))
                {
                    const std::string *address { Get<std::string>(first, "address") };
                    out = std::string(address ? *address : "");
                }

                while (sd_bus_message_skip(m, "a{sv}") > 0) {}
                sd_bus_message_exit_container(m);
            }
        }
        else
            sd_bus_message_skip(m, contents);

        sd_bus_message_exit_container(m);
    }

    // At an a{sv}
    bool ReadDict(sd_bus_message *m, Dict &out) noexcept
    {
        if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0)
            return false;

        while (sd_bus_message_enter_container(m, 'e', "sv") > 0)
        {
            const char *key {};

            if (sd_bus_message_read(m, "s", &key) < 0)
                break;

            ReadVariant(m, out[key ? key : ""]);
            sd_bus_message_exit_container(m);
        }

        sd_bus_message_exit_container(m);
        return true;
    }

    Dict GetAll(sd_bus *bus, const std::string &path, const char *iface) noexcept
    {
        Dict d;
        sd_bus_message *reply {};

        if (sd_bus_call_method(bus, NM, path.c_str(), "org.freedesktop.DBus.Properties", "GetAll", nullptr, &reply, "s", iface) >= 0)
            ReadDict(reply, d);

        sd_bus_message_unref(reply);
        return d;
    }

    std::vector<std::string> CallPaths(sd_bus *bus, const char *path, const char *iface, const char *method) noexcept
    {
        std::vector<std::string> list;
        sd_bus_message *reply {};

        if (sd_bus_call_method(bus, NM, path, iface, method, nullptr, &reply, "") >= 0 &&
            sd_bus_message_enter_container(reply, 'a', "o") >= 0)
        {
            const char *p {};

            while (sd_bus_message_read(reply, "o", &p) > 0)
                list.emplace_back(p);
        }

        sd_bus_message_unref(reply);
        return list;
    }

    Settings GetSettings(sd_bus *bus, const std::string &connection) noexcept
    {
        Settings settings;
        sd_bus_message *reply {};

        if (sd_bus_call_method(bus, NM, connection.c_str(), NMConnection, "GetSettings", nullptr, &reply, "") >= 0 &&
            sd_bus_message_enter_container(reply, 'a', "{sa{sv}}") >= 0)
        {
            while (sd_bus_message_enter_container(reply, 'e', "sa{sv}") > 0)
            {
                const char *group {};

                if (sd_bus_message_read(reply, "s", &group) < 0)
                    break;

                ReadDict(reply, settings[group ? group : ""]);
                sd_bus_message_exit_container(reply);
            }
        }

        sd_bus_message_unref(reply);
        return settings;
    }

    std::string SSID(const std::vector<UInt8> *bytes) noexcept
    {
        return bytes ? std::string(bytes->begin(), bytes->end()) : std::string();
    }

    // Reads everything the pane shows (worker thread).
    WiFiState ReadState(sd_bus *bus) noexcept
    {
        WiFiState s;
        s.loaded = true;

        if (!bus)
            return s;

        const Dict nm { GetAll(bus, NMPath, NM) };

        if (nm.empty())
            return s; // No NetworkManager

        s.enabled = GetOr<bool>(nm, "WirelessEnabled", false);

        // The first Wi-Fi device
        Dict device;

        for (const auto &path : CallPaths(bus, NMPath, NM, "GetDevices"))
        {
            Dict d { GetAll(bus, path, NMDevice) };

            if (GetOr<UInt64>(d, "DeviceType", 0) == 2 /* Wi-Fi */)
            {
                s.device = path;
                device = std::move(d);
                break;
            }
        }

        if (s.device.empty())
            return s;

        s.available = true;
        s.interface = GetOr<std::string>(device, "Interface", "");
        s.deviceState = UInt32(GetOr<UInt64>(device, "State", 0));

        if (const auto *reason { Get<std::vector<UInt64>>(device, "StateReason") }; reason && reason->size() == 2)
            s.stateReason = UInt32((*reason)[1]);

        const Dict wireless { GetAll(bus, s.device, NMWireless) };
        s.hwAddress = GetOr<std::string>(wireless, "HwAddress", GetOr<std::string>(device, "HwAddress", ""));

        // Saved Wi-Fi profiles, by name
        struct Profile { std::string path, uuid; bool autoJoin; };
        std::unordered_map<std::string, Profile> profiles;

        for (const auto &path : CallPaths(bus, NMSettingsPath, NMSettings, "ListConnections"))
        {
            const Settings settings { GetSettings(bus, path) };
            const auto con { settings.find("connection") };
            const auto wifi { settings.find("802-11-wireless") };

            if (con == settings.end() || wifi == settings.end() ||
                GetOr<std::string>(con->second, "type", "") != "802-11-wireless")
                continue;

            const std::string ssid { SSID(Get<std::vector<UInt8>>(wifi->second, "ssid")) };

            if (!ssid.empty())
                profiles[ssid] = { path, GetOr<std::string>(con->second, "uuid", ""), GetOr<bool>(con->second, "autoconnect", true) };
        }

        // Access points in range, grouped by name (the strongest one each)
        const std::string *activeAP { Get<std::string>(wireless, "ActiveAccessPoint") };

        if (const auto *aps { Get<std::vector<std::string>>(wireless, "AccessPoints") })
        {
            std::unordered_map<std::string, size_t> byName;

            for (const auto &ap : *aps)
            {
                const Dict a { GetAll(bus, ap, NMAccessPoint) };
                const std::string ssid { SSID(Get<std::vector<UInt8>>(a, "Ssid")) };

                if (ssid.empty())
                    continue; // Hidden

                if (activeAP && *activeAP == ap)
                    s.activeSsid = ssid;

                const UInt8 strength { UInt8(GetOr<UInt64>(a, "Strength", 0)) };
                const auto it { byName.find(ssid) };

                if (it != byName.end() && s.networks[it->second].strength >= strength)
                    continue;

                WiFiNetwork n;
                n.ssid = ssid;
                n.strength = strength;
                n.accessPoint = ap;

                const UInt64 flags { GetOr<UInt64>(a, "Flags", 0) };
                const UInt64 wpa { GetOr<UInt64>(a, "WpaFlags", 0) }, rsn { GetOr<UInt64>(a, "RsnFlags", 0) };
                n.secured = (flags & APPrivacy) || wpa || rsn;
                n.enterprise = ((wpa | rsn) & KeyMgmt8021X) != 0;
                n.sae = (rsn & KeyMgmtSAE) && !((wpa | rsn) & KeyMgmtPSK);
                n.wep = (flags & APPrivacy) && !wpa && !rsn;

                if (const auto p { profiles.find(ssid) }; p != profiles.end())
                {
                    n.connection = p->second.path;
                    n.uuid = p->second.uuid;
                    n.autoJoin = p->second.autoJoin;
                }

                if (it != byName.end())
                    s.networks[it->second] = std::move(n);
                else
                {
                    byName[ssid] = s.networks.size();
                    s.networks.emplace_back(std::move(n));
                }
            }
        }

        // Strongest first (by name when equally strong)
        std::sort(s.networks.begin(), s.networks.end(), [](const auto &a, const auto &b) {
            if (a.strength != b.strength)
                return a.strength > b.strength;
            const auto lower = [](std::string x){ std::transform(x.begin(), x.end(), x.begin(), ::tolower); return x; };
            return lower(a.ssid) < lower(b.ssid);
        });

        if (s.deviceState == DeviceActivated)
            s.link = WiFiState::Link::Connected;
        else if (s.deviceState >= DevicePrepare && s.deviceState < DeviceActivated)
            s.link = WiFiState::Link::Connecting;

        if (s.link == WiFiState::Link::Disconnected)
            s.activeSsid.clear();

        // Its address and router
        if (s.link == WiFiState::Link::Connected)
            if (const std::string *ip4 { Get<std::string>(device, "Ip4Config") }; ip4 && ip4->size() > 1)
            {
                const Dict ip { GetAll(bus, *ip4, "org.freedesktop.NetworkManager.IP4Config") };
                s.ipAddress = GetOr<std::string>(ip, "AddressData", "");
                s.router = GetOr<std::string>(ip, "Gateway", "");
            }

        return s;
    }

    bool Run(std::initializer_list<const char*> args) noexcept
    {
        std::vector<char*> argv;

        for (const char *a : args)
            argv.push_back(const_cast<char*>(a));

        argv.push_back(nullptr);
        pid_t pid;

        if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
            return false;

        int status {};
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
}

/* ------------------------------------------ WiFi ------------------------------------------ */

std::string WiFiNetwork::securityName() const noexcept
{
    if (!secured) return "None";
    if (enterprise) return "WPA2 Enterprise";
    if (wep) return "WEP";
    if (sae) return "WPA3 Personal";
    return "WPA2 Personal";
}

const WiFiNetwork *WiFiState::active() const noexcept
{
    if (activeSsid.empty())
        return nullptr;

    for (const auto &n : networks)
        if (n.ssid == activeSsid)
            return &n;

    return nullptr;
}

WiFi::WiFi() noexcept : m_worker(CZBusWorker::System)
{
    m_refreshTimer = std::make_unique<CZTimer>([this](CZTimer*){ refresh(); });
    // Until networks show up: scan again (the device may not have been ready) and read them
    m_scanRetry = std::make_unique<CZTimer>([this](CZTimer*){
        if (!searching())
            return;

        scan();
        refresh();
        m_scanRetry->start(2000);
    });

    // Any NetworkManager property change (devices, access points, connections...)
    m_system = CZBus::GetOrMakeSystem();

    if (m_system)
        sd_bus_match_signal_async(m_system->bus(), &m_match, NM, nullptr, "org.freedesktop.DBus.Properties", "PropertiesChanged",
            [](sd_bus_message *, void *data, sd_bus_error *) -> int {
                auto *w { static_cast<WiFi*>(data) };

                if (!w->m_refreshTimer->running())
                    w->m_refreshTimer->start(400);
                return 0;
            }, nullptr, this);

    refresh();
}

WiFi::~WiFi() noexcept
{
    sd_bus_slot_unref(m_match); // Before the connection (m_system) goes
}

void WiFi::refresh() noexcept
{
    // At most one read running, and one more queued
    if (m_busy)
    {
        m_again = true;
        return;
    }

    m_busy = true;
    m_worker.run([this](sd_bus *bus) -> std::function<void()> {
        WiFiState state { ReadState(bus) };

        return [this, state = std::move(state)]() mutable {
            m_busy = false;
            apply(std::move(state));

            if (m_again)
            {
                m_again = false;
                refresh();
            }
        };
    });
}

void WiFi::apply(WiFiState state) noexcept
{
    m_state = std::move(state);

    if (m_scanPending && !m_state.device.empty())
        scan();

    // On without networks (e.g. just turned on: the device rejects scans until it's ready)
    if (searching())
    {
        if (!m_scanRetry->running())
            m_scanRetry->start(2000);
    }
    else
        m_scanRetry->stop();

    // A join in progress: done, or rejected
    if (m_joining)
    {
        const Int64 elapsed { Int64(CZTime::Ms()) - m_joining->startMs };
        const UInt32 st { m_state.deviceState }, reason { m_state.stateReason };

        if (st >= DevicePrepare && st < DeviceActivated)
            m_joining->sawProgress = true;

        if (m_state.link == WiFiState::Link::Connected && m_state.activeSsid == m_joining->ssid)
            m_joining.reset();
        else if ((m_joining->sawProgress || elapsed > 1500) && (st == DeviceFailed || st == DeviceDisconnected) && elapsed > 300)
        {
            // NO_SECRETS, SUPPLICANT_DISCONNECT/CONFIG_FAILED/FAILED/TIMEOUT: the password
            const bool wrongPassword { reason >= 7 && reason <= 11 };
            const auto joining { std::move(m_joining) };

            if (!joining->newConnection.empty())
            {
                const std::string path { joining->newConnection };
                m_worker.run([path](sd_bus *bus) -> std::function<void()> {
                    if (bus)
                        sd_bus_call_method(bus, NM, path.c_str(), NMConnection, "Delete", nullptr, nullptr, "");
                    return {};
                });
            }

            onJoinFailed.notify(joining->ssid, wrongPassword);
        }
        else if (elapsed > 60000)
            m_joining.reset();
    }

    onChanged.notify();
}

void WiFi::setEnabled(bool enabled) noexcept
{
    m_state.enabled = enabled; // Shown right away

    // Its networks are listed again only after a scan
    if (!enabled)
        m_state.networks.clear();

    onChanged.notify();
    m_worker.run([enabled](sd_bus *bus) -> std::function<void()> {
        sd_bus_error error = SD_BUS_ERROR_NULL;

        if (bus && sd_bus_set_property(bus, NM, NMPath, NM, "WirelessEnabled", &error, "b", int(enabled)) < 0)
            CZLog(CZWarning, CZLN, "Can't turn Wi-Fi {}: {}", enabled ? "on" : "off", error.message ? error.message : "");

        sd_bus_error_free(&error);
        return {};
    });
    refresh();
}

void WiFi::scan() noexcept
{
    // Done once it's known (see apply())
    if (m_state.device.empty())
    {
        m_scanPending = !m_state.loaded || m_state.available;
        return;
    }

    m_scanPending = false;
    m_worker.run([device = m_state.device](sd_bus *bus) -> std::function<void()> {
        sd_bus_error error = SD_BUS_ERROR_NULL;

        // Rejected while one is running or right after another (fine: the list refreshes anyway)
        if (bus && sd_bus_call_method(bus, NM, device.c_str(), NMWireless, "RequestScan", &error, nullptr, "a{sv}", 0) < 0)
            CZLog(CZDebug, CZLN, "Wi-Fi scan not started: {}", error.message ? error.message : "");

        sd_bus_error_free(&error);
        return {};
    });
}

void WiFi::join(const WiFiNetwork &network, const std::string &password) noexcept
{
    if (m_state.device.empty())
        return;

    m_joining = std::make_unique<Joining>();
    m_joining->ssid = network.ssid;
    m_joining->startMs = Int64(CZTime::Ms());

    // Only the completion (main thread) uses this
    m_worker.run([this, network, password, device = m_state.device](sd_bus *bus) -> std::function<void()> {
        if (!bus)
            return {};

        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *reply {};
        std::string created;

        if (network.known() && password.empty())
            sd_bus_call_method(bus, NM, NMPath, NM, "ActivateConnection", &error, &reply, "ooo",
                               network.connection.c_str(), device.c_str(), "/");
        else
        {
            // A new profile (NetworkManager fills the rest from the access point)
            sd_bus_message *call {};
            sd_bus_message_new_method_call(bus, &call, NM, NMPath, NM, "AddAndActivateConnection");

            if (network.secured)
                sd_bus_message_append(call, "a{sa{sv}}", 1, "802-11-wireless-security", 2,
                                      "key-mgmt", "s", network.sae ? "sae" : "wpa-psk",
                                      "psk", "s", password.c_str());
            else
                sd_bus_message_append(call, "a{sa{sv}}", 0);

            sd_bus_message_append(call, "oo", device.c_str(), network.accessPoint.c_str());

            if (sd_bus_call(bus, call, 0, &error, &reply) >= 0)
            {
                const char *path {};

                if (sd_bus_message_read(reply, "o", &path) >= 0 && path)
                    created = path;
            }

            sd_bus_message_unref(call);
        }

        const bool failed { sd_bus_error_is_set(&error) != 0 };

        if (failed)
            CZLog(CZWarning, CZLN, "Can't join {}: {}", network.ssid, error.message ? error.message : "");

        sd_bus_error_free(&error);
        sd_bus_message_unref(reply);

        return [this, ssid = network.ssid, created, failed]{
            if (!m_joining || m_joining->ssid != ssid)
                return;

            if (failed)
            {
                m_joining.reset();
                onJoinFailed.notify(ssid, false);
                return;
            }

            m_joining->newConnection = created;
            refresh();
        };
    });
}

void WiFi::forget(const WiFiNetwork &network) noexcept
{
    if (!network.known())
        return;

    m_worker.run([path = network.connection](sd_bus *bus) -> std::function<void()> {
        if (bus)
            sd_bus_call_method(bus, NM, path.c_str(), NMConnection, "Delete", nullptr, nullptr, "");
        return {};
    });
    refresh();
}

void WiFi::setAutoJoin(const WiFiNetwork &network, bool autoJoin) noexcept
{
    if (network.uuid.empty())
        return;

    // Through nmcli: NetworkManager's Update() replaces the whole profile (secrets included)
    m_worker.run([uuid = network.uuid, autoJoin](sd_bus *) -> std::function<void()> {
        if (!Run({ "nmcli", "connection", "modify", "uuid", uuid.c_str(), "connection.autoconnect", autoJoin ? "yes" : "no" }))
            CZLog(CZWarning, CZLN, "nmcli couldn't change Auto-Join of {}", uuid);
        return {};
    });
    refresh();
}
