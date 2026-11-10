#include <Menus/AppMenu.h>
#include <Models/MenuModel.h>
#include <Nodes/Action.h>
#include <Core/Firmament.h>
#include <Apps/AppActions.h>
#include <Heaven/Bar/HNBar.h>
#include <Heaven/Bar/HNClient.h>
#include <sys/types.h>
#include <signal.h>

using namespace CZ;

AppMenu::AppMenu() noexcept
{
    m_menu = newMenu();

    m_about = addAction(m_menu, "About");

    addDivider(m_menu);
    m_settings = addAction(m_menu, "Settings…", "⌘,");

    addDivider(m_menu);
    addSubMenu(m_menu, "Services"); // Left empty for now (later: D-Bus services from apps).

    addDivider(m_menu);
    m_hide = addAction(m_menu, "Hide", "⌘H");
    auto *hideOthers { addAction(m_menu, "Hide Others", "⌥⌘H") };
    auto *showAll { addAction(m_menu, "Show All") };

    addDivider(m_menu);
    m_quit = addAction(m_menu, "Quit", "⌘Q");

    setAppName({});

    // About / Settings / Quit target the active Heaven client, or the D-Bus actions of an
    // unregistered app.
    m_about->onClicked = []{
        auto *firm { GetFirm() };
        if (auto *c { firm->heaven->activeClient() }) c->about();
        else if (firm->appActions->enabled("about")) firm->appActions->activate("about");
    };

    m_settings->onClicked = []{
        auto *firm { GetFirm() };
        if (auto *c { firm->heaven->activeClient() }) c->settings();
        else if (firm->appActions->enabled("preferences")) firm->appActions->activate("preferences");
    };

    // Quit a Heaven client via its quit event, an unregistered app via its D-Bus quit action or by
    // signalling its pid, and as a last resort close its window.
    m_quit->onClicked = []{
        auto *firm { GetFirm() };
        const auto &heaven { firm->heaven };
        if (auto *c { heaven->activeClient() })
            c->quit();
        else if (firm->appActions->enabled("quit"))
            firm->appActions->activate("quit");
        else if (const UInt32 pid { heaven->activeClientPid() }; pid != 0)
            ::kill(static_cast<pid_t>(pid), SIGTERM);
        else
            heaven->closeActiveClient();
    };

    // Hide / Hide Others / Show All are handled by the compositor.
    m_hide->onClicked  = []{ GetFirm()->heaven->hideActiveClient(); };
    hideOthers->onClicked = []{ GetFirm()->heaven->hideOtherClients(); };
    showAll->onClicked = []{ GetFirm()->heaven->showAllClients(); };
}

void AppMenu::setAppName(const std::string &name) noexcept
{
    const std::string suffix { name.empty() ? std::string {} : " " + name };
    m_about->setText("About" + suffix + "…");
    m_hide->setText("Hide" + suffix);
    m_quit->setText("Quit" + suffix);
}

void AppMenu::setNativeMode(bool native) noexcept
{
    const auto &actions { GetFirm()->appActions };
    m_about->setEnabled(!native || actions->enabled("about"));
    m_settings->setEnabled(!native || actions->enabled("preferences"));
}
