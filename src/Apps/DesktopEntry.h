#ifndef DESKTOPENTRY_H
#define DESKTOPENTRY_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief The subset of a .desktop file (freedesktop Desktop Entry Specification) Firmament uses.
 */
struct DesktopEntry
{
    /// An "Additional application action" ([Desktop Action <id>] group listed in Actions=).
    struct Action
    {
        std::string id;
        std::string name;   ///< Localized
        std::string exec;   ///< Raw Exec value (may be empty for D-Bus activatable apps)
    };

    std::filesystem::path path;
    std::string id;         ///< Desktop file id (basename without ".desktop")
    std::string name;       ///< Localized Name
    std::string icon;
    bool dbusActivatable { false };
    std::vector<Action> actions;

    /**
     * @brief Finds the entry matching an application id.
     *
     * Looks for `<appId>.desktop` (and its lowercase variant, which covers most X11 WM_CLASS values)
     * in `$XDG_DATA_HOME/applications` and every `$XDG_DATA_DIRS/applications`. Results (including
     * misses) are cached.
     *
     * @return The entry, or nullptr if none was found.
     */
    static std::shared_ptr<const DesktopEntry> Find(const std::string &appId) noexcept;

    /**
     * @brief Launches @p exec (an Exec value) detached, expanding/stripping its field codes.
     *
     * @return true if the process was spawned.
     */
    bool launch(const std::string &exec) const noexcept;
};

#endif // DESKTOPENTRY_H
