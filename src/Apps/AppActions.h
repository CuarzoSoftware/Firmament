#ifndef APPACTIONS_H
#define APPACTIONS_H

#include <XDG/XDGDesktopEntry.h>
#include <Menus/MenuBuilder.h>
#include <Models/TopbarModel.h>
#include <CZ/Core/CZObject.h>
#include <CZ/Core/CZSignal.h>
#include <memory>
#include <string>
#include <unordered_map>

#include <systemd/sd-bus.h>

/**
 * @brief Discovers the actions an unregistered (non-Heaven) app exposes and maps the common ones to
 *        menus.
 *
 * Two sources are combined, both keyed by the app id the compositor reports for the active window:
 *
 * 1. **GTK / GApplication actions** (`org.gtk.Actions`). GApplication-based apps own their app id
 *    as a well-known name on the session bus and export their application actions at the object
 *    path derived from it (e.g. "org.gnome.TextEditor" -> "/org/gnome/TextEditor"). They are listed
 *    asynchronously with `DescribeAll` (never auto-starting the app), kept in sync via the `Changed`
 *    signal and activated with `org.gtk.Actions.Activate`. Only parameterless actions are used:
 *     - about, preferences, quit           -> the app-title menu (see AppMenu)
 *     - new-window, new-document, open     -> "File" menu
 *     - help, shortcuts                    -> "Help" menu
 *
 * 2. **freedesktop Desktop Entry actions** ("Additional application actions": the `Actions=` key and
 *    `[Desktop Action <id>]` groups of the app's .desktop file). The spec has no D-Bus method to
 *    list them, so they're read from the .desktop file and activated through
 *    `org.freedesktop.Application.ActivateAction` when the app is `DBusActivatable`, or by running
 *    the action's `Exec` otherwise. They're appended to the "File" menu (they're usually things like
 *    "New Window" / "New Private Window"), skipping those already provided by (1).
 *
 * The resulting menu bar (topbar()) also includes the native "Window" menu.
 */
class AppActions final : public CZ::CZObject
{
public:
    AppActions() noexcept;
    ~AppActions() noexcept;

    /// Starts tracking the app identified by @p appId (empty: stops tracking). No-op if it's already
    /// the tracked app. Must not be called while topbar() is being shown (it's destroyed). Doesn't
    /// emit onChanged: topbar() already reflects the .desktop actions when it returns, and GTK
    /// actions arrive later (asynchronously) through onChanged.
    void setAppId(const std::string &appId) noexcept;
    const std::string &appId() const noexcept { return m_appId; }

    /// The tracked app's .desktop entry, if found.
    const std::shared_ptr<const XDGDesktopEntry> &desktopEntry() const noexcept { return m_entry; }

    /// Whether the app exports the parameterless GTK action @p name (it may be disabled).
    bool has(const std::string &name) const noexcept { return m_gtkActions.contains(name); }

    /// Whether the app exports the parameterless GTK action @p name and it's currently enabled.
    bool enabled(const std::string &name) const noexcept;

    /// Asks the app to activate its GTK action @p name.
    void activate(const std::string &name) noexcept;

    /// Activates the .desktop file action @p id.
    void activateDesktopAction(const std::string &id) noexcept;

    /// The menu bar built from the discovered actions, or nullptr if none of the menu-bar mapped
    /// actions are available.
    TopbarModel *topbar() const noexcept;

    /// Emitted after the action set or any enabled state changes. If topbar() changed, the previous
    /// one is destroyed right after this signal is emitted, so it must not be referenced afterwards.
    CZ::CZSignal<> onChanged;

    /// True if @p name is a valid D-Bus well-known name.
    static bool IsValidBusName(const std::string &name) noexcept;

private:
    struct Menus;

    void reset() noexcept;
    void query() noexcept;
    void rebuild(bool notify = true) noexcept;
    void update(std::unordered_map<std::string, bool> &&actions) noexcept;
    static int DescribeAllReply(sd_bus_message *m, void *userdata, sd_bus_error *err);
    static int ChangedSignal(sd_bus_message *m, void *userdata, sd_bus_error *err);

    std::string m_appId;
    std::shared_ptr<const XDGDesktopEntry> m_entry;
    std::string m_busName;  // Well-known name the app (may) own, empty if none
    std::string m_path;     // Its GApplication / org.freedesktop.Application object path
    std::unordered_map<std::string, bool> m_gtkActions; // parameterless actions -> enabled
    std::unique_ptr<Menus> m_menus;
    sd_bus_slot *m_querySlot {};
    sd_bus_slot *m_changedSlot {};
};

#endif // APPACTIONS_H
