#include <Core/Audio.h>
#include <Core/Log.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

using namespace CZ;

// The virtual devices routing to some channels of another one (see Audio::select())
static constexpr const char *OutputRemap { "cz_channels_output" };
static constexpr const char *InputRemap { "cz_channels_input" };

/* ------------------------------------------ Helpers ------------------------------------------ */

// The environment with LC_ALL=C (untranslated output to parse)
static std::vector<char*> CEnv() noexcept
{
    std::vector<char*> env;

    for (char **e = environ; *e; e++)
        if (strncmp(*e, "LC_ALL=", 7) != 0)
            env.push_back(*e);

    env.push_back(const_cast<char*>("LC_ALL=C"));
    env.push_back(nullptr);
    return env;
}

/**
 * Starts @p args with its stdout in a pipe (if @p out isn't nullptr, its read end, non-blocking if
 * @p nonBlocking) and stderr discarded. Returns the pid, -1 on failure.
 */
static pid_t Spawn(const std::vector<std::string> &args, int *out, bool nonBlocking = false) noexcept
{
    std::vector<char*> argv;

    for (const auto &a : args)
        argv.push_back(const_cast<char*>(a.c_str()));

    argv.push_back(nullptr);

    int fds[2] { -1, -1 };

    if (out && pipe2(fds, O_CLOEXEC) != 0)
        return -1;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);

    if (out)
        posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    else
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);

    auto env { CEnv() };
    pid_t pid;
    const int ret { posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), env.data()) };
    posix_spawn_file_actions_destroy(&actions);

    if (out)
    {
        close(fds[1]);

        if (ret != 0)
        {
            close(fds[0]);
            return -1;
        }

        if (nonBlocking)
            fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);

        *out = fds[0];
    }

    return ret == 0 ? pid : -1;
}

// Runs a command and waits for it (worker thread only). Its stdout, empty if it failed.
static std::string Run(const std::vector<std::string> &args, bool *ok = nullptr) noexcept
{
    int fd { -1 };
    const pid_t pid { Spawn(args, &fd) };

    if (ok)
        *ok = false;

    if (pid < 0)
        return {};

    std::string out;
    char buf[4096];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
        out.append(buf, size_t(n));

    close(fd);

    int status { 0 };
    waitpid(pid, &status, 0);

    if (ok)
        *ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;

    return out;
}

// Stops a child we don't wait for (reaped by a detached thread)
static void Stop(pid_t pid) noexcept
{
    if (pid <= 0)
        return;

    kill(pid, SIGTERM);
    std::thread([pid]{ waitpid(pid, nullptr, 0); }).detach();
}

static std::string Trim(std::string s) noexcept
{
    const auto b { s.find_first_not_of(" \t\r\n") };

    if (b == std::string::npos)
        return {};

    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

/**
 * Parses `pactl list sinks` (or sources, @p input), in the C locale. Monitors of outputs are skipped.
 *
 * Sink #671
 *     Name: alsa_output...
 *     Description: Raptor Lake High Definition Audio Controller Speaker
 *     Mute: no
 *     Volume: front-left: 36863 /  56% / -14.99 dB,   front-right: 36863 /  56% / -14.99 dB
 *     Properties:
 *         device.bus = "pci"
 *     Active Port: [Out] Speaker
 */
static std::vector<AudioDevice> ParseDevices(const std::string &text, bool input) noexcept
{
    struct Raw
    {
        AudioDevice dev;
        std::string description, bus, api, cls, deviceDescription, port, factory, icon;
        bool virtualNode { false };
    };

    std::vector<Raw> raws;
    std::istringstream in { text };
    std::string line;
    bool inVolume { false };
    const std::string header { input ? "Source #" : "Sink #" };

    const auto prop = [](const std::string &line, const char *key, std::string &value) -> bool {
        const std::string k { std::string(key) + " = \"" };
        const auto p { line.find(k) };

        if (p == std::string::npos)
            return false;

        value = line.substr(p + k.size());

        if (!value.empty() && value.back() == '"')
            value.pop_back();

        return true;
    };

    while (std::getline(in, line))
    {
        if (line.rfind(header, 0) == 0)
        {
            raws.emplace_back();
            inVolume = false;
            continue;
        }

        if (raws.empty())
            continue;

        Raw &r { raws.back() };
        const std::string t { Trim(line) };
        std::string v;

        if (t.rfind("Name: ", 0) == 0)
            r.dev.name = t.substr(6);
        else if (t.rfind("Description: ", 0) == 0)
            r.description = t.substr(13);
        else if (t.rfind("Mute: ", 0) == 0)
            r.dev.muted = t.substr(6) == "yes";
        else if (t.rfind("Volume: ", 0) == 0 && !inVolume)
        {
            // The channels' percents, averaged
            inVolume = true;
            float sum { 0.f };
            int n { 0 };
            size_t pos { 0 };

            while ((pos = t.find('%', pos)) != std::string::npos)
            {
                size_t b { pos };

                while (b > 0 && std::isdigit(static_cast<unsigned char>(t[b - 1])))
                    b--;

                if (b < pos)
                {
                    sum += float(std::atoi(t.substr(b, pos - b).c_str()));
                    n++;
                }

                pos++;
            }

            r.dev.volume = n > 0 ? sum / float(n) / 100.f : 0.f;
        }
        else if (t.rfind("Active Port: ", 0) == 0)
            r.port = t.substr(13);
        else if (t.rfind("Channel Map: ", 0) == 0)
        {
            std::istringstream channels { t.substr(13) };
            std::string channel;

            while (std::getline(channels, channel, ','))
                r.dev.channels.push_back(Trim(channel));
        }
        else if (prop(t, "cz.master", v))
            r.dev.remapMaster = v;
        else if (prop(t, "cz.channels", v))
        {
            std::istringstream list { v };
            std::string n;

            while (std::getline(list, n, ','))
                r.dev.remapChannels.push_back(UInt32(std::atoi(n.c_str())));
        }
        else if (prop(t, "device.bus", v))
            r.bus = v;
        else if (prop(t, "device.api", v))
            r.api = v;
        else if (prop(t, "device.class", v))
            r.cls = v;
        else if (prop(t, "device.description", v))
            r.deviceDescription = v;
        else if (prop(t, "factory.name", v))
            r.factory = v;
        else if (prop(t, "device.icon_name", v))
            r.icon = v;
        else if (prop(t, "node.virtual", v))
            r.virtualNode = v == "true";
    }

    std::vector<AudioDevice> devices;

    for (Raw &r : raws)
    {
        if (r.dev.name.empty() || r.cls == "monitor" || r.dev.name.ends_with(".monitor"))
            continue;

        AudioDevice &d { r.dev };
        const bool hdmi { r.port.find("HDMI") != std::string::npos || r.description.find("HDMI") != std::string::npos };

        if (r.bus == "bluetooth" || r.api == "bluez5")
            d.type = "Bluetooth";
        else if (r.bus == "usb")
            d.type = "USB";
        else if (hdmi)
            d.type = "HDMI";
        else if (r.api.empty() || r.virtualNode || r.factory.find("null") != std::string::npos)
            d.type = "Virtual";
        else
            d.type = "Built-in";

        if (!d.remapMaster.empty())
        {
            d.type = "Virtual";
            d.displayName = r.description;
            devices.push_back(std::move(d));
            continue;
        }

        // Built-in ones: the part after the controller ("… Controller Speaker" → "Speaker")
        d.displayName = r.description;

        if ((d.type == "Built-in" || d.type == "HDMI") && !r.deviceDescription.empty() &&
            r.description.size() > r.deviceDescription.size() + 1 && r.description.rfind(r.deviceDescription + " ", 0) == 0)
            d.displayName = r.description.substr(r.deviceDescription.size() + 1);

        if (d.displayName.empty())
            d.displayName = d.name;

        devices.push_back(std::move(d));
    }

    return devices;
}

/* ----------------------------------------- AudioState ----------------------------------------- */

const AudioDevice *AudioState::find(bool input, const std::string &name) const noexcept
{
    for (const auto &d : input ? inputs : outputs)
        if (d.name == name)
            return &d;

    return nullptr;
}

std::string AudioState::ChannelsName(bool input, const std::vector<UInt32> &channels) noexcept
{
    if (channels.empty())
        return {};

    if (channels.size() == 1)
        return "Channel " + std::to_string(channels[0] + 1);

    std::string name { input ? "Channels " : "Channels " };

    for (size_t i = 0; i < channels.size(); i++)
        name += (i ? "+" : "") + std::to_string(channels[i] + 1);

    return name;
}

/* ------------------------------------------- Audio ------------------------------------------- */

// Where the chosen channels are remembered (recreated if missing, e.g. after logging in again)
static std::filesystem::path SelectionFile() noexcept
{
    const char *config { getenv("XDG_CONFIG_HOME") };
    const char *home { getenv("HOME") };
    const std::filesystem::path dir { config && *config ? std::filesystem::path(config) :
                                      std::filesystem::path(home ? home : "/tmp") / ".config" };
    return dir / "cz-firmament" / "audio-channels";
}

Audio::Audio() noexcept
{
    m_refreshTimer = std::make_unique<CZTimer>([this](CZTimer*){ refresh(); });
    m_retryTimer = std::make_unique<CZTimer>([this](CZTimer*){ startMonitor(); });

    // Saved: "output <device> <channel>,<channel>"
    std::ifstream file { SelectionFile() };
    std::string direction, device, channels;

    while (file >> direction >> device >> channels)
    {
        Saved &saved { direction == "input" ? m_savedInput : m_savedOutput };
        saved.device = device;
        std::istringstream list { channels };
        std::string n;

        while (std::getline(list, n, ','))
            saved.channels.push_back(UInt32(std::atoi(n.c_str())));
    }

    startMonitor();
    refresh();
}

Audio::~Audio() noexcept
{
    stopMonitor();
}

void Audio::refresh() noexcept
{
    // Coalesced: one running, at most one more queued
    if (m_busy)
    {
        m_again = true;
        return;
    }

    m_busy = true;

    m_worker.run([this](sd_bus*) -> std::function<void()> {
        AudioState s;
        bool ok;
        const std::string sinks { Run({ "pactl", "list", "sinks" }, &ok) };
        s.available = ok;

        if (ok)
        {
            auto outputs { ParseDevices(sinks, false) };
            auto inputs { ParseDevices(Run({ "pactl", "list", "sources" }), true) };
            const std::string defaultOutput { Trim(Run({ "pactl", "get-default-sink" })) };
            const std::string defaultInput { Trim(Run({ "pactl", "get-default-source" })) };

            // Our remaps aren't listed: the device they route to is, with the channels chosen
            const auto split = [](std::vector<AudioDevice> &all, const std::string &def, AudioSelection &sel,
                                  std::vector<AudioDevice> &out) {
                for (auto &d : all)
                {
                    if (!d.remapMaster.empty())
                    {
                        if (d.name == def)
                        {
                            sel = { d.remapMaster, d.remapChannels, d.volume, d.muted, true };
                            sel.node = d.name;
                        }
                        continue;
                    }

                    out.push_back(d);
                }

                if (sel.device.empty())
                    for (const auto &d : out)
                        if (d.name == def)
                            sel = { d.name, {}, d.volume, d.muted, true, d.name };
            };

            split(outputs, defaultOutput, s.output, s.outputs);
            split(inputs, defaultInput, s.input, s.inputs);
        }

        s.loaded = true;

        return [this, s = std::move(s)]() mutable {
            m_busy = false;
            m_state = std::move(s);
            restoreSaved();
            onChanged.notify();

            if (m_again)
            {
                m_again = false;
                refresh();
            }
        };
    });
}

void Audio::restoreSaved() noexcept
{
    // Once: the remap of the channels chosen last time, if missing (e.g. a new session) and its
    // device is there
    if (m_restored || !m_state.available)
        return;

    m_restored = true;

    for (bool input : { false, true })
    {
        const Saved &saved { input ? m_savedInput : m_savedOutput };
        const AudioSelection &current { input ? m_state.input : m_state.output };

        if (saved.device.empty() || saved.channels.empty() || !m_state.find(input, saved.device))
            continue;

        if (current.device == saved.device && current.channels == saved.channels)
            continue;

        select(input, saved.device, saved.channels);
    }
}

void Audio::save() noexcept
{
    std::error_code ec;
    std::filesystem::create_directories(SelectionFile().parent_path(), ec);
    std::ofstream file { SelectionFile(), std::ios::trunc };

    for (const auto &[direction, saved] : { std::pair { "output", &m_savedOutput }, std::pair { "input", &m_savedInput } })
    {
        if (saved->device.empty() || saved->channels.empty())
            continue;

        std::string channels;

        for (size_t i = 0; i < saved->channels.size(); i++)
            channels += (i ? "," : "") + std::to_string(saved->channels[i]);

        file << direction << ' ' << saved->device << ' ' << channels << '\n';
    }
}

void Audio::select(bool input, const std::string &device, std::vector<UInt32> channels) noexcept
{
    const AudioDevice *dev { m_state.find(input, device) };

    if (!dev)
        return;

    // Only existing channels
    std::erase_if(channels, [dev](UInt32 c){ return c >= dev->channels.size(); });

    // Shown right away (confirmed by the refresh)
    AudioSelection &sel { input ? m_state.input : m_state.output };
    sel.device = device;
    sel.channels = channels;
    onChanged.notify();

    Saved &saved { input ? m_savedInput : m_savedOutput };
    saved = { device, channels };
    save();

    std::vector<std::string> masterMap;

    for (UInt32 c : channels)
        masterMap.push_back(dev->channels[c]);

    std::string description { dev->displayName + " (" + AudioState::ChannelsName(input, channels) + ")" };
    std::erase_if(description, [](char c){ return c == '"' || c == '\'' || c == '\\'; });

    m_worker.run([this, input, device, channels, masterMap, description](sd_bus*) -> std::function<void()> {
        const char *remap { input ? InputRemap : OutputRemap };

        // The previous remap (one per direction)
        std::istringstream modules { Run({ "pactl", "list", "modules", "short" }) };
        std::string line;

        while (std::getline(modules, line))
            if (line.find(std::string(input ? "source_name=" : "sink_name=") + remap) != std::string::npos)
                Run({ "pactl", "unload-module", line.substr(0, line.find_first_of(" \t")) });

        std::string node { device };

        if (!channels.empty())
        {
            std::string masterChannels, ids;

            for (size_t i = 0; i < masterMap.size(); i++)
            {
                masterChannels += (i ? "," : "") + masterMap[i];
                ids += (i ? "," : "") + std::to_string(channels[i]);
            }

            // Its channels: a stereo pair, or a mono input
            const std::string map { input ? (channels.size() == 1 ? "mono" : "front-left,front-right") :
                                            (channels.size() == 1 ? "mono" : "front-left,front-right") };
            const std::string props { std::string(input ? "source_properties=" : "sink_properties=") +
                "'device.description=\"" + description + "\" cz.master=\"" + device + "\" cz.channels=\"" + ids + "\"'" };

            bool ok;
            Run({ "pactl", "load-module", input ? "module-remap-source" : "module-remap-sink",
                  std::string(input ? "source_name=" : "sink_name=") + remap,
                  "master=" + device,
                  "channels=" + std::to_string(channels.size()),
                  "master_channel_map=" + masterChannels,
                  "channel_map=" + map,
                  "remix=no",
                  props }, &ok);

            if (ok)
                node = remap;
            else
                Log(CZWarning, CZLN, "Couldn't route to the channels {} of {}", ids, device);
        }

        Run({ "pactl", input ? "set-default-source" : "set-default-sink", node });
        return [this]{ refresh(); };
    });
}

void Audio::setVolume(bool input, float volume) noexcept
{
    AudioSelection &sel { input ? m_state.input : m_state.output };

    if (sel.node.empty())
        return;

    sel.volume = volume;
    m_pendingVolume[input] = volume;

    if (m_volumeBusy)
        return;

    applyVolume();
}

void Audio::applyVolume() noexcept
{
    if (m_pendingVolume[0] < 0.f && m_pendingVolume[1] < 0.f)
        return;

    m_volumeBusy = true;
    const float out { m_pendingVolume[0] }, in { m_pendingVolume[1] };
    m_pendingVolume[0] = m_pendingVolume[1] = -1.f;

    m_worker.run([this, out, in](sd_bus*) -> std::function<void()> {
        const auto percent = [](float v){ return std::to_string(std::lround(std::clamp(v, 0.f, 1.5f) * 100.f)) + "%"; };

        if (out >= 0.f)
        {
            Run({ "pactl", "set-sink-volume", "@DEFAULT_SINK@", percent(out) });
            Run({ "pactl", "set-sink-mute", "@DEFAULT_SINK@", out > 0.f ? "0" : "1" });
        }

        if (in >= 0.f)
        {
            Run({ "pactl", "set-source-volume", "@DEFAULT_SOURCE@", percent(in) });
            Run({ "pactl", "set-source-mute", "@DEFAULT_SOURCE@", in > 0.f ? "0" : "1" });
        }

        return [this]{
            m_volumeBusy = false;
            applyVolume();
        };
    });
}

/* ------------------------------------------ Monitor ------------------------------------------ */

void Audio::startMonitor() noexcept
{
    stopMonitor();

    // A line per server event, e.g. "Event 'change' on sink #56"
    int fd { -1 };
    m_monitorPid = Spawn({ "pactl", "subscribe" }, &fd, true);

    if (m_monitorPid < 0)
    {
        m_monitorPid = -1;
        Log(CZWarning, CZLN, "Failed to run pactl subscribe: audio changes won't be tracked");
        return;
    }

    m_monitorSource = CZEventSource::Make(fd, EPOLLIN, CZOwn::Own, [this](int fd, UInt32 events, CZEventSource *source) {
        char buf[1024];
        ssize_t n;
        bool changed { false };

        while ((n = read(fd, buf, sizeof(buf))) > 0)
            m_monitorBuffer.append(buf, size_t(n));

        size_t nl;

        while ((nl = m_monitorBuffer.find('\n')) != std::string::npos)
        {
            const std::string line { m_monitorBuffer.substr(0, nl) };
            m_monitorBuffer.erase(0, nl + 1);

            // Not the clients/streams (e.g. our own parec coming and going)
            if (line.find(" sink ") != std::string::npos || line.find(" source ") != std::string::npos ||
                line.find(" server") != std::string::npos || line.find(" card ") != std::string::npos)
                changed = true;
        }

        if (changed && !m_refreshTimer->running())
            m_refreshTimer->start(150);

        // pactl quit (e.g. the audio server restarted): again in a while
        if (n == 0 || (events & (EPOLLHUP | EPOLLERR)))
        {
            source->setEvents(0); // Stop polling the dead pipe until restarted (startMonitor())
            m_retryTimer->start(3000);
            m_refreshTimer->start(150);
        }
    });
}

void Audio::stopMonitor() noexcept
{
    m_monitorSource.reset();
    m_monitorBuffer.clear();
    Stop(m_monitorPid);
    m_monitorPid = -1;
}

