#include <Apps/DesktopEntry.h>
#include <Core/Firmament.h>
#include <Core/Log.h>
#include <XDG/XDGINI.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/close_range.h>
#include <sys/syscall.h>

namespace fs = std::filesystem;

using Section = CZ::XDGMap<std::string, std::string>;

static std::vector<fs::path> ApplicationDirs() noexcept
{
    std::vector<fs::path> dirs;
    const auto &kit { GetFirm()->xdgkit };

    if (const char *dataHome { getenv("XDG_DATA_HOME") }; dataHome && dataHome[0] == '/')
        dirs.emplace_back(fs::path(dataHome) / "applications");
    else if (kit && !kit->homeDir().empty())
        dirs.emplace_back(kit->homeDir() / ".local/share/applications");

    if (kit)
        for (const auto &dir : kit->dataDirs())
            dirs.emplace_back(dir / "applications");

    return dirs;
}

// Locale suffixes to try for localized keys, most specific first (e.g. "es_CL.UTF-8" -> es_CL, es).
static const std::vector<std::string> &LocaleKeys() noexcept
{
    static const std::vector<std::string> keys { []{
        std::vector<std::string> keys;
        const char *env { nullptr };

        for (const char *var : { "LC_ALL", "LC_MESSAGES", "LANG" })
            if ((env = getenv(var)) && env[0] != '\0')
                break;

        if (!env || env[0] == '\0')
            return keys;

        std::string locale { env };
        locale = locale.substr(0, locale.find_first_of(".@"));

        if (locale.empty() || locale == "C" || locale == "POSIX")
            return keys;

        keys.emplace_back(locale);

        if (const auto underscore { locale.find('_') }; underscore != std::string::npos)
            keys.emplace_back(locale.substr(0, underscore));

        return keys;
    }() };

    return keys;
}

static std::string Value(const Section &section, const std::string &key) noexcept
{
    auto it { section.find(key) };
    return it != section.end() ? it->second : std::string {};
}

static std::string LocalizedValue(const Section &section, const std::string &key) noexcept
{
    for (const auto &locale : LocaleKeys())
        if (auto value { Value(section, key + "[" + locale + "]") }; !value.empty())
            return value;

    return Value(section, key);
}

// Splits a ';'-separated string list (Desktop Entry spec "string(s)" type).
static std::vector<std::string> SplitList(const std::string &list) noexcept
{
    std::vector<std::string> out;
    std::string cur;

    for (size_t i = 0; i < list.size(); i++)
    {
        if (list[i] == '\\' && i + 1 < list.size() && list[i + 1] == ';')
            cur += list[++i];
        else if (list[i] == ';')
        {
            if (!cur.empty()) out.emplace_back(std::move(cur));
            cur.clear();
        }
        else
            cur += list[i];
    }

    if (!cur.empty())
        out.emplace_back(std::move(cur));

    return out;
}

static std::shared_ptr<DesktopEntry> Load(const fs::path &file) noexcept
{
    auto ini { CZ::XDGINI::LoadFile(file) };
    auto main { ini->find("Desktop Entry") };

    if (main == ini->end() || Value(main->second, "Type") != "Application" || Value(main->second, "Hidden") == "true")
        return {};

    auto entry { std::make_shared<DesktopEntry>() };
    entry->path = file;
    entry->id = file.stem().string();
    entry->name = LocalizedValue(main->second, "Name");
    entry->icon = Value(main->second, "Icon");
    entry->dbusActivatable = Value(main->second, "DBusActivatable") == "true";

    for (const auto &id : SplitList(Value(main->second, "Actions")))
    {
        auto group { ini->find("Desktop Action " + id) };

        if (group == ini->end())
            continue;

        DesktopEntry::Action action { id, LocalizedValue(group->second, "Name"), Value(group->second, "Exec") };

        if (!action.name.empty() && (entry->dbusActivatable || !action.exec.empty()))
            entry->actions.emplace_back(std::move(action));
    }

    return entry;
}

std::shared_ptr<const DesktopEntry> DesktopEntry::Find(const std::string &appId) noexcept
{
    static std::unordered_map<std::string, std::shared_ptr<const DesktopEntry>> cache;

    if (appId.empty() || appId.find('/') != std::string::npos)
        return {};

    if (auto it { cache.find(appId) }; it != cache.end())
        return it->second;

    std::string lower { appId };
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });

    std::shared_ptr<const DesktopEntry> entry;

    for (const auto &dir : ApplicationDirs())
    {
        for (const auto &candidate : { appId, lower })
        {
            std::error_code ec;
            const fs::path file { dir / (candidate + ".desktop") };

            if (fs::is_regular_file(file, ec))
                entry = Load(file);

            if (entry)
                break;
        }

        if (entry)
            break;
    }

    cache[appId] = entry;
    return entry;
}

// Splits an Exec value into arguments (Desktop Entry spec quoting rules) and expands field codes.
static std::vector<std::string> ExecArgs(const DesktopEntry &entry, const std::string &exec) noexcept
{
    std::vector<std::string> args;
    std::string cur;
    bool inArg { false }, quoted { false };

    for (size_t i = 0; i < exec.size(); i++)
    {
        const char c { exec[i] };

        if (quoted)
        {
            if (c == '\\' && i + 1 < exec.size() && std::strchr("\"`$\\", exec[i + 1]))
                cur += exec[++i];
            else if (c == '"')
                quoted = false;
            else
                cur += c;
        }
        else if (c == '"')
        {
            quoted = inArg = true;
        }
        else if (c == ' ' || c == '\t')
        {
            if (inArg) args.emplace_back(std::move(cur));
            cur.clear();
            inArg = false;
        }
        else
        {
            cur += c;
            inArg = true;
        }
    }

    if (inArg)
        args.emplace_back(std::move(cur));

    std::vector<std::string> out;

    for (const auto &arg : args)
    {
        // Codes that expand to whole arguments.
        if (arg == "%i") { if (!entry.icon.empty()) { out.emplace_back("--icon"); out.emplace_back(entry.icon); } continue; }

        std::string expanded;
        bool drop { false };

        for (size_t i = 0; i < arg.size(); i++)
        {
            if (arg[i] != '%' || i + 1 >= arg.size()) { expanded += arg[i]; continue; }

            switch (arg[++i])
            {
            case '%': expanded += '%'; break;
            case 'c': expanded += entry.name; break;
            case 'k': expanded += entry.path.string(); break;
            // No files/URLs are passed: an argument consisting only of these is removed.
            case 'f': case 'F': case 'u': case 'U': if (arg.size() == 2) drop = true; break;
            default: break; // deprecated / unknown codes are removed
            }
        }

        if (!drop)
            out.emplace_back(std::move(expanded));
    }

    return out;
}

bool DesktopEntry::launch(const std::string &exec) const noexcept
{
    const auto args { ExecArgs(*this, exec) };

    if (args.empty())
        return false;

    // Prepared before forking: only async-signal-safe calls happen in the child.
    std::vector<char*> argv;
    for (const auto &a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    Log(CZDebug, CZLN, "[DesktopEntry] Launching {} action: {}", id, exec);

    const pid_t child { fork() };

    if (child < 0)
        return false;

    if (child == 0)
    {
        // Double fork so the app is reparented to init and never becomes our zombie.
        setsid();
        if (fork() != 0) _exit(0);
        syscall(SYS_close_range, 3, ~0U, 0);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    waitpid(child, nullptr, 0);
    return true;
}
