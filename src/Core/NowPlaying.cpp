#include <Core/NowPlaying.h>
#include <Core/Log.h>
#include <systemd/sd-bus.h>
#include <filesystem>
#include <functional>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <spawn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

extern char **environ;

namespace fs = std::filesystem;

static constexpr const char *MprisPrefix { "org.mpris.MediaPlayer2." };
static constexpr const char *MprisPath { "/org/mpris/MediaPlayer2" };
static constexpr const char *MprisRoot { "org.mpris.MediaPlayer2" };
static constexpr const char *MprisPlayer { "org.mpris.MediaPlayer2.Player" };

static fs::path ArtworkDir() noexcept
{
    const char *cache { getenv("XDG_CACHE_HOME") };
    const char *home { getenv("HOME") };
    const fs::path dir { cache && *cache ? fs::path(cache) : fs::path(home ? home : "/tmp") / ".cache" };
    return dir / "cz-firmament" / "artwork";
}

// %XX decoded (file:// URLs)
static std::string UrlDecode(const std::string &s) noexcept
{
    std::string out;

    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == '%' && i + 2 < s.size())
        {
            out += char(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        }
        else
            out += s[i];
    }

    return out;
}

// The artwork of @p url as a local file: file:// as is, http(s) downloaded once (worker thread).
static std::string ArtworkFile(const std::string &url) noexcept
{
    if (url.rfind("file://", 0) == 0)
        return UrlDecode(url.substr(7));

    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
        return {};

    std::error_code ec;
    const fs::path dir { ArtworkDir() };
    const fs::path file { dir / std::to_string(std::hash<std::string>{}(url)) };

    if (fs::is_regular_file(file, ec) && fs::file_size(file, ec) > 0)
        return file;

    fs::create_directories(dir, ec);

    // Not too many kept
    if (std::distance(fs::directory_iterator(dir, ec), fs::directory_iterator()) > 200)
        for (const auto &e : fs::directory_iterator(dir, ec))
            fs::remove(e.path(), ec);

    const std::string tmp { file.string() + ".tmp" };
    const char *argv[] { "curl", "-sfL", "--max-time", "8", "--max-filesize", "10000000", "-o", tmp.c_str(), url.c_str(), nullptr };
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid;
    const int ret { posix_spawnp(&pid, "curl", &actions, nullptr, const_cast<char**>(argv), environ) };
    posix_spawn_file_actions_destroy(&actions);

    if (ret != 0)
        return {};

    int status { 0 };
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        fs::remove(tmp, ec);
        return {};
    }

    fs::rename(tmp, file, ec);
    return ec ? std::string() : file.string();
}

NowPlaying::NowPlaying() noexcept
{
    m_refreshTimer = std::make_unique<CZTimer>([this](CZTimer*){ refresh(); });
    m_bus = CZBus::GetOrMakeUser();

    if (m_bus)
    {
        // A player changed (playing, track...), or appeared / went away
        const auto changed = [](sd_bus_message *, void *data, sd_bus_error *) -> int {
            auto *self { static_cast<NowPlaying*>(data) };

            if (!self->m_refreshTimer->running())
                self->m_refreshTimer->start(100);
            return 0;
        };

        sd_bus_match_signal_async(m_bus->bus(), &m_propertiesMatch, nullptr, MprisPath,
            "org.freedesktop.DBus.Properties", "PropertiesChanged", changed, nullptr, this);
        sd_bus_add_match_async(m_bus->bus(), &m_ownerMatch,
            "type='signal',sender='org.freedesktop.DBus',member='NameOwnerChanged',arg0namespace='org.mpris.MediaPlayer2'",
            changed, nullptr, this);
    }

    refresh();
}

NowPlaying::~NowPlaying() noexcept
{
    sd_bus_slot_unref(m_propertiesMatch);
    sd_bus_slot_unref(m_ownerMatch);
}

void NowPlaying::refresh() noexcept
{
    // Coalesced: one running, at most one more queued
    if (m_busy)
    {
        m_again = true;
        return;
    }

    m_busy = true;

    m_worker.run([this, lastPlaying = m_lastPlaying](sd_bus *bus) -> std::function<void()> {
        struct Player { State state; std::string artUrl; };
        std::vector<Player> players;
        char **names {};

        if (bus && sd_bus_list_names(bus, &names, nullptr) >= 0 && names)
        {
            for (char **name = names; *name; name++)
            {
                // playerctld mirrors the others
                if (strncmp(*name, MprisPrefix, strlen(MprisPrefix)) == 0 && !strstr(*name, "playerctld"))
                {
                    Player p;
                    p.state.available = true;
                    p.state.player = *name;
                    char *str {};

                    if (sd_bus_get_property_string(bus, *name, MprisPath, MprisPlayer, "PlaybackStatus", nullptr, &str) >= 0 && str)
                        p.state.playing = strcmp(str, "Playing") == 0;
                    free(str);
                    str = nullptr;

                    if (sd_bus_get_property_string(bus, *name, MprisPath, MprisRoot, "Identity", nullptr, &str) >= 0 && str)
                        p.state.identity = str;
                    free(str);

                    int b { 0 };
                    if (sd_bus_get_property_trivial(bus, *name, MprisPath, MprisPlayer, "CanGoNext", nullptr, 'b', &b) >= 0) p.state.canNext = b;
                    if (sd_bus_get_property_trivial(bus, *name, MprisPath, MprisPlayer, "CanGoPrevious", nullptr, 'b', &b) >= 0) p.state.canPrevious = b;
                    if (sd_bus_get_property_trivial(bus, *name, MprisPath, MprisPlayer, "CanPlay", nullptr, 'b', &b) >= 0) p.state.canPlayPause = b;
                    if (!p.state.canPlayPause && sd_bus_get_property_trivial(bus, *name, MprisPath, MprisPlayer, "CanPause", nullptr, 'b', &b) >= 0) p.state.canPlayPause = b;
                    if (sd_bus_get_property_trivial(bus, *name, MprisPath, MprisRoot, "CanRaise", nullptr, 'b', &b) >= 0) p.state.canRaise = b;

                    // Metadata: a{sv} with xesam:title (s), xesam:artist (as), mpris:artUrl (s)
                    sd_bus_message *reply {};

                    if (sd_bus_get_property(bus, *name, MprisPath, MprisPlayer, "Metadata", nullptr, &reply, "a{sv}") >= 0)
                    {
                        if (sd_bus_message_enter_container(reply, 'a', "{sv}") > 0)
                        {
                            while (sd_bus_message_enter_container(reply, 'e', "sv") > 0)
                            {
                                const char *key {};
                                sd_bus_message_read(reply, "s", &key);
                                const std::string k { key ? key : "" };
                                const char *contents {};
                                sd_bus_message_peek_type(reply, nullptr, &contents);

                                if ((k == "xesam:title" || k == "mpris:artUrl") && contents && strcmp(contents, "s") == 0)
                                {
                                    sd_bus_message_enter_container(reply, 'v', "s");
                                    const char *v {};
                                    sd_bus_message_read(reply, "s", &v);
                                    (k == "xesam:title" ? p.state.title : p.artUrl) = v ? v : "";
                                    sd_bus_message_exit_container(reply);
                                }
                                else if (k == "xesam:artist" && contents && strcmp(contents, "as") == 0)
                                {
                                    sd_bus_message_enter_container(reply, 'v', "as");
                                    sd_bus_message_enter_container(reply, 'a', "s");
                                    const char *v {};

                                    while (sd_bus_message_read(reply, "s", &v) > 0)
                                        p.state.artist += (p.state.artist.empty() ? "" : ", ") + std::string(v ? v : "");

                                    sd_bus_message_exit_container(reply);
                                    sd_bus_message_exit_container(reply);
                                }
                                else
                                    sd_bus_message_skip(reply, "v");

                                sd_bus_message_exit_container(reply);
                            }
                        }

                        sd_bus_message_unref(reply);
                    }

                    players.push_back(std::move(p));
                }

                free(*name);
            }

            free(names);
        }

        // The one playing, else the last that did, else one with something to show
        const Player *chosen { nullptr };

        for (const auto &p : players)
            if (p.state.playing) { chosen = &p; break; }

        if (!chosen)
            for (const auto &p : players)
                if (p.state.player == lastPlaying) { chosen = &p; break; }

        if (!chosen)
            for (const auto &p : players)
                if (!p.state.title.empty()) { chosen = &p; break; }

        State state;
        std::string artUrl;

        if (chosen)
        {
            state = chosen->state;
            artUrl = chosen->artUrl;
        }

        // Shown right away, the artwork once downloaded (a second step: the download takes a moment)
        return [this, state, artUrl]() mutable {
            m_busy = false;

            if (state.playing)
                m_lastPlaying = state.player;

            // Same track: the artwork already there stays
            if (state.player == m_state.player && state.title == m_state.title)
                state.artwork = m_state.artwork;

            m_state = state;
            onChanged.notify();

            if (!artUrl.empty() && m_state.artwork.empty())
            {
                m_worker.run([this, artUrl, player = state.player, title = state.title](sd_bus*) -> std::function<void()> {
                    std::string file { ArtworkFile(artUrl) };
                    return [this, file, player, title]{
                        if (m_state.player == player && m_state.title == title && m_state.artwork != file)
                        {
                            m_state.artwork = file;
                            onChanged.notify();
                        }
                    };
                });
            }

            if (m_again)
            {
                m_again = false;
                refresh();
            }
        };
    });
}

void NowPlaying::call(const char *method, bool root) noexcept
{
    if (m_state.player.empty())
        return;

    m_worker.run([player = m_state.player, method, root](sd_bus *bus) -> std::function<void()> {
        if (bus && sd_bus_call_method(bus, player.c_str(), MprisPath, root ? MprisRoot : MprisPlayer, method, nullptr, nullptr, "") < 0)
            Log(CZWarning, CZLN, "{} of {} failed", method, player);
        return {};
    });
}

void NowPlaying::playPause() noexcept
{
    // Shown right away (confirmed by its signal)
    m_state.playing = !m_state.playing;
    onChanged.notify();
    call("PlayPause");
}

void NowPlaying::previous() noexcept { call("Previous"); }
void NowPlaying::next() noexcept { call("Next"); }
void NowPlaying::raise() noexcept { call("Raise", true); }
