#include <Menus/AppMenu.h>
#include <Models/MenuModel.h>
#include <Nodes/Action.h>
#include <Core/Firmament.h>
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
    auto *settings { addAction(m_menu, "Settings…", "⌘,") };

    addDivider(m_menu);
    addSubMenu(m_menu, "Services"); // Left empty for now (later: D-Bus services from apps).

    addDivider(m_menu);
    m_hide = addAction(m_menu, "Hide", "⌘H");
    auto *hideOthers { addAction(m_menu, "Hide Others", "⌥⌘H") };
    auto *showAll { addAction(m_menu, "Show All") };

    addDivider(m_menu);
    m_quit = addAction(m_menu, "Quit", "⌘Q");

    setAppName({});

    // About / Settings / Quit target the active client.
    m_about->onClicked = []{ if (auto *c { GetFirm()->heaven->activeClient() }) c->about(); };
    settings->onClicked = []{ if (auto *c { GetFirm()->heaven->activeClient() }) c->settings(); };

    // Quit a Heaven client via its quit event, or a native app by signalling its pid.
    m_quit->onClicked = []{
        const auto &heaven { GetFirm()->heaven };
        if (auto *c { heaven->activeClient() })
            c->quit();
        else if (const UInt32 pid { heaven->activeClientPid() }; pid != 0)
            ::kill(static_cast<pid_t>(pid), SIGTERM);
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
    m_about->setEnabled(!native);
}
