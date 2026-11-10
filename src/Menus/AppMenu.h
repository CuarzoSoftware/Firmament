#ifndef APPMENU_H
#define APPMENU_H

#include <Menus/MenuBuilder.h>
#include <string>

/**
 * @brief The default application menu, opened from the bold app-title item on every topbar.
 *
 * macOS-style layout:
 *   About <App>… · — · Settings… · — · Services ▸ · — · Hide <App> / Hide Others / Show All · — · Quit <App>
 *
 * About/Settings/Quit are forwarded to the active Heaven client; Hide/Hide Others/Show All are sent
 * to the compositor. The About/Hide/Quit labels track the active application name (see setAppName()).
 */
class AppMenu final : public MenuBuilder
{
public:
    AppMenu() noexcept;

    MenuModel *menu() const noexcept { return m_menu; }

    /// Updates the "About <App>…", "Hide <App>" and "Quit <App>" labels with the active app name.
    void setAppName(const std::string &name) noexcept;

    /// Native (non-Heaven) app mode: About/Settings/Quit are routed to the app's D-Bus actions
    /// (about/preferences/quit, see AppActions) when it exports them. Otherwise About and Settings
    /// are disabled (there is no client to ask), and Quit sends a termination signal to the app's pid
    /// (or closes its window if the pid is unknown) instead of a Heaven quit event.
    void setNativeMode(bool native) noexcept;

private:
    MenuModel *m_menu {};
    Action *m_about {};
    Action *m_settings {};
    Action *m_hide {};
    Action *m_quit {};
};

#endif // APPMENU_H
