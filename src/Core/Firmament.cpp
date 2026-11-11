#include <Core/Firmament.h>
#include <Core/Log.h>
#include <Marco/MScreen.h>
#include <fstream>
#include <cctype>
#include <chrono>
#include <locale.h>
#include <ctime>

#include <Models/TopbarModel.h>
#include <Models/MenuModel.h>
#include <Surfaces/TopbarSurface.h>
#include <Surfaces/MenuSurface.h>
#include <Nodes/MenuItemsContainer.h>
#include <Nodes/TopbarItem.h>
#include <Nodes/MenuItem.h>
#include <Nodes/Action.h>
#include <Nodes/Toggle.h>
#include <Nodes/Divider.h>
#include <Nodes/SubMenu.h>
#include <Menus/SystemMenu.h>
#include <Menus/ScaleMenu.h>
#include <Core/Status.h>
#include <Surfaces/ControlPanel.h>
#include <Core/Controls.h>
#include <Core/CZTime.h>
#include <XDG/XDGINI.h>
#include <filesystem>
#include <cmath>
#include <Menus/DeskMenus.h>
#include <Menus/AppMenu.h>
#include <Menus/NativeMenus.h>
#include <Apps/AppActions.h>
#include <XDG/XDGDesktopEntry.h>

#include <Heaven/Bar/HNClient.h>
#include <Heaven/Bar/HNTopbar.h>
#include <Heaven/Bar/HNMenu.h>
#include <Heaven/Bar/HNToggle.h>
#include <Heaven/Bar/HNWithParent.h>
#include <Heaven/Bar/HNWithTitle.h>
#include <Heaven/Bar/HNWithIcon.h>
#include <Heaven/Bar/HNWithShortcut.h>
#include <Heaven/Bar/HNWithEnabled.h>

static Firmament *_firm;

Firmament *GetFirm() noexcept { return _firm; }

// --- Heaven object helpers ------------------------------------------------

static HNObject *ParentOf(HNObject *o) noexcept
{
    auto *w { dynamic_cast<HNWithParent*>(o) };
    return w ? w->parent() : nullptr;
}

static std::string hnTitle(HNObject *o) noexcept
{
    auto *w { dynamic_cast<HNWithTitle*>(o) };
    return w ? w->title() : std::string {};
}

static std::string hnIcon(HNObject *o) noexcept
{
    auto *w { dynamic_cast<HNWithIcon*>(o) };
    return w ? w->icon() : std::string {};
}

static std::string hnShortcut(HNObject *o) noexcept
{
    auto *w { dynamic_cast<HNWithShortcut*>(o) };
    return w ? w->shortcut() : std::string {};
}

static bool hnEnabled(HNObject *o) noexcept
{
    auto *w { dynamic_cast<HNWithEnabled*>(o) };
    return w ? w->enabled() : true;
}

// Node representing an action/toggle/divider (their node is stored directly in userData).
static MenuItem *itemNode(HNObject *o) noexcept
{
    switch (o->type())
    {
    case HNObject::Action:
    case HNObject::Toggle:
    case HNObject::Divider:
        return static_cast<MenuItem*>(o->userData);
    default:
        return nullptr;
    }
}

// The AK node an object contributes to its parent MENU's popup content.
static AKNode *nodeFor(HNObject *o) noexcept
{
    if (o->type() == HNObject::Menu)
    {
        auto *m { MenuModel::From(o) };
        return m ? m->subMenuItem() : nullptr;
    }
    return itemNode(o);
}

// --------------------------------------------------------------------------

Firmament::Firmament() noexcept
{
    _firm = this;

    marco = MApp::Make();
    if (!marco)
        return;

    xdgkit = XDGKit::Make();
    if (!xdgkit)
        return;

    assets.init();

    loadSettings();
    systemMenu = std::make_unique<SystemMenu>();
    scaleMenu = std::make_unique<ScaleMenu>();
    scaleMenu->update();
    deskMenus = std::make_unique<DeskMenus>();
    appMenu = std::make_unique<AppMenu>();
    nativeMenus = std::make_unique<NativeMenus>();
    appActions = std::make_unique<AppActions>();
    m_activeSource = &deskMenus->topbar; // Desk by default

    // Before the bars (they show it).
    status = std::make_unique<Status>();
    status->onChanged.subscribe(this, [this]{
        Log(CZDebug, CZLN, "Status: {}, {}, {}", status->networkInfo, status->volumeInfo, status->batteryInfo);

        for (MScreen *s : marco->screens())
            if (auto *tb { static_cast<TopbarSurface*>(s->userData) })
                tb->updateStatus();

        if (controlPanel && controlPanel->mapped())
            controlPanel->updateStatus();
    });

    controls = std::make_unique<Controls>();
    controls->onChanged.subscribe(this, [this]{
        if (controlPanel && controlPanel->mapped())
            controlPanel->updateStatus();
    });

    // Dark mode changed elsewhere (e.g. GNOME's settings): the tile follows.
    marco->onColorSchemeChanged.subscribe(this, [this](CZColorScheme){
        if (controlPanel && controlPanel->mapped())
            controlPanel->updateStatus();
    });

    initScreens();
    initHeaven();

    m_clockTimer = std::make_unique<CZTimer>([this](CZTimer *){ updateClock(); scheduleClock(); });
    scheduleClock();
}

Firmament::~Firmament() noexcept {}

// --------------------------------------------------------------------------
// Clock
// --------------------------------------------------------------------------

static std::filesystem::path SettingsPath() noexcept
{
    std::filesystem::path dir;

    if (const char *config { getenv("XDG_CONFIG_HOME") }; config && config[0] == '/')
        dir = config;
    else if (const char *home { getenv("HOME") }; home && home[0] == '/')
        dir = std::filesystem::path(home) / ".config";

    return dir / "cz-firmament" / "firmament.conf";
}

void Firmament::loadSettings() noexcept
{
    std::error_code ec;

    if (!std::filesystem::is_regular_file(SettingsPath(), ec))
        return;

    auto ini { XDGINI::LoadFile(SettingsPath()) };

    if (!ini)
        return;

    if (auto group { ini->find("Topbar") }; group != ini->end())
        if (auto it { group->second.find("Scale") }; it != group->second.end())
        {
            const float value { std::strtof(it->second.c_str(), nullptr) };

            for (const float scale : TOPBAR_SCALES)
                if (std::fabs(scale - value) < 0.01f)
                    m_uiScale = scale;
        }
}

void Firmament::saveSettings() noexcept
{
    const auto path { SettingsPath() };
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    const auto tmp { path.string() + ".tmp" };
    {
        std::ofstream out { tmp, std::ios::trunc };

        if (!out)
            return;

        out << "[Topbar]\nScale=" << m_uiScale << '\n';
    }

    std::filesystem::rename(tmp, path, ec);
}

void Firmament::setUIScale(float scale) noexcept
{
    if (scale == m_uiScale)
        return;

    m_uiScale = scale;
    saveSettings();
    setActiveTopbarItem(nullptr);
    scaleMenu->update();

    for (MScreen *s : marco->screens())
        if (auto *tb { static_cast<TopbarSurface*>(s->userData) })
            tb->updateUIScale();
}


std::string Firmament::clockText() const noexcept
{
    const time_t now { time(nullptr) };
    tm local;
    localtime_r(&now, &local);

    // The user's LC_TIME (without changing the process locale).
    static const locale_t loc { newlocale(LC_TIME_MASK, "", (locale_t)0) };
    const auto format = [&local](const char *fmt) {
        char buf[64];
        const size_t n { loc ? strftime_l(buf, sizeof(buf), fmt, &local, loc) : strftime(buf, sizeof(buf), fmt, &local) };
        return std::string(buf, n);
    };

    std::string weekday { format("%a") };
    std::string month { format("%b") };
    const std::string day { format("%-d") };
    const std::string hour { format("%H:%M") };

    // Weekday capitalized, like the macOS menu bar ("mar" → "Mar").
    if (!weekday.empty() && (unsigned char)weekday[0] < 0x80)
        weekday[0] = std::toupper((unsigned char)weekday[0]);

    std::string lang;

    for (const char *var : { "LC_ALL", "LC_TIME", "LANG" })
        if (const char *v { getenv(var) }; v && v[0])
        {
            lang = v;
            break;
        }

    // Spanish: day before month and the abbreviation's period ("Mar 10 nov. 11:52").
    if (lang.starts_with("es"))
    {
        if (!month.empty() && month.back() != '.' && month != format("%B"))
            month += '.';

        return weekday + " " + day + " " + month + " " + hour;
    }

    return weekday + " " + month + " " + day + " " + hour;
}

void Firmament::updateClock() noexcept
{
    const std::string text { clockText() };

    for (MScreen *s : marco->screens())
        if (auto *tb { static_cast<TopbarSurface*>(s->userData) })
            tb->clock.setText(text);
}

void Firmament::scheduleClock() noexcept
{
    // Right after the next minute starts.
    const auto now { std::chrono::system_clock::now().time_since_epoch() };
    const auto ms { std::chrono::duration_cast<std::chrono::milliseconds>(now).count() };
    m_clockTimer->start(UInt32(60000 - ms % 60000 + 50));
}

void Firmament::initScreens() noexcept
{
    marco->onScreenPlugged.subscribe(this, [this](MScreen &s){ onScreenPlugged(s); });
    marco->onScreenUnplugged.subscribe(this, [this](MScreen &s){ onScreenUnplugged(s); });

    for (MScreen *s : marco->screens())
        onScreenPlugged(*s);
}

void Firmament::onScreenPlugged(MScreen &screen) noexcept
{
    if (screen.userData)
        return; // Already has a topbar

    Log(CZInfo, CZLN, "Screen plugged: {} ({} screen(s))", screen.props().name, marco->screens().size());
    new TopbarSurface(screen); // Sets screen.userData and renders the active menu bar

    // Refresh the app title (and app menu) on the new bar.
    if (heaven)
        updateActiveClient();
}

void Firmament::onScreenUnplugged(MScreen &screen) noexcept
{
    auto *topbar { static_cast<TopbarSurface*>(screen.userData) };
    screen.userData = nullptr;

    // The screen is still listed while this signal is emitted.
    const size_t remaining { marco->screens().size() - 1 };
    Log(CZInfo, CZLN, "Screen unplugged: {} ({} screen(s) left{})", screen.props().name, remaining,
        remaining == 0 ? ", e.g. the compositor switched VT; waiting for screens to come back" : "");

    if (!topbar)
        return;

    // Close the open topbar menu if it hangs from this bar (its popups can't outlive their parent).
    // Done for every pooled menu, since the one being replaced may still be mapped.
    bool closeMenus { activeTopbarItem && activeTopbarItem->scene() && activeTopbarItem->scene()->window() == topbar };

    for (auto *menu : { m_menuA.get(), m_menuB.get() })
        if (menu && menu->parent() == topbar)
            closeMenus = true;

    if (closeMenus)
    {
        setActiveTopbarItem(nullptr);

        for (auto *menu : { m_menuA.get(), m_menuB.get() })
        {
            if (menu && menu->parent() == topbar)
            {
                menu->setMapped(false);
                menu->setParent(nullptr);
            }
        }
    }

    // Drop the per-screen topbar nodes every menu created for this bar.
    MenuModel::RemoveTopbarItems(topbar);
    delete topbar;
}

// --- Topbar rendering -----------------------------------------------------

// The ordered top-level menus of a source: read live from the Heaven topbar's children when it is
// Heaven-backed, or the static list for the built-in "Desk" bar.
static std::vector<MenuModel*> effectiveMenus(TopbarModel *src) noexcept
{
    std::vector<MenuModel*> menus;

    if (!src)
        return menus;

    if (auto *hn { src->hnTopbar() })
    {
        for (HNObject *child : hn->children())
            if (auto *m { MenuModel::From(child) })
                menus.push_back(m);
    }
    else
    {
        const auto &v { src->menus() };
        menus.assign(v.begin(), v.end());
    }

    return menus;
}

void Firmament::renderTopbar(TopbarSurface *surface) noexcept
{
    auto &area { surface->menusArea };

    while (!area.children().empty())
        area.children().back()->setParent(nullptr);

    for (MenuModel *m : effectiveMenus(m_activeSource))
    {
        TopbarItem *item { m->topbarItem(surface) };
        item->setParent(&area);
        item->setUIScale(surface->uiScale());
    }
}

// --- Hierarchy logging ----------------------------------------------------

static std::string indent(int depth) noexcept { return std::string(depth * 2, ' '); }

// Logs a single menu item node (and, for submenus, recurses into their content).
static void logItemNode(AKNode *n, int depth) noexcept
{
    if (auto *sm = dynamic_cast<SubMenu*>(n))
    {
        Log(CZDebug, "{}> Menu: '{}'", indent(depth), sm->text.text());
        if (sm->submenu)
            for (AKNode *child : sm->submenu->content()->children())
                logItemNode(child, depth + 1);
    }
    else if (auto *a = dynamic_cast<Action*>(n))
    {
        if (a->shortcut)
            Log(CZDebug, "{}- Action: '{}'   [{}]", indent(depth), a->text.text(), a->shortcut->text());
        else
            Log(CZDebug, "{}- Action: '{}'", indent(depth), a->text.text());
    }
    else if (auto *t = dynamic_cast<Toggle*>(n))
    {
        Log(CZDebug, "{}- Toggle: [{}] '{}'", indent(depth), t->icon.visible() ? "x" : " ", t->text.text());
    }
    else if (auto *d = dynamic_cast<Divider*>(n))
    {
        if (d->text)
            Log(CZDebug, "{}- Divider: '{}'", indent(depth), d->text->text());
        else
            Log(CZDebug, "{}- Divider", indent(depth));
    }
}

static void logMenuModel(MenuModel *m, int depth) noexcept
{
    if (!m)
        return;

    Log(CZDebug, "{}> Menu: '{}'", indent(depth), m->title());

    for (AKNode *child : m->content()->children())
        logItemNode(child, depth + 1);
}

void Firmament::logMenuHierarchy() noexcept
{
    // Resolve the source currently shown by the topbars: the active client's menu bar, or Desk.
    std::string appName { "Desk (built-in)" };

    if (auto *client { heaven ? heaven->activeClient() : nullptr })
        appName = "'" + client->name() + "' (id " + client->id() + ")";

    std::vector<MenuModel*> menus { effectiveMenus(m_activeSource) };

    Log(CZDebug, "===== Menu hierarchy =====");
    Log(CZDebug, "Client / topbar: {}", appName);
    Log(CZDebug, "Top-level menus: {}", menus.size());

    for (MenuModel *m : menus)
        logMenuModel(m, 1);

    Log(CZDebug, "==========================");
}

void Firmament::renderAllTopbars() noexcept
{
    if (!marco)
        return;

    for (MScreen *s : marco->screens())
        if (auto *tb { static_cast<TopbarSurface*>(s->userData) })
            renderTopbar(tb);
}

void Firmament::setActiveSource(TopbarModel *source) noexcept
{
    m_activeSource = source;
    renderAllTopbars();
}

void Firmament::reRenderIfActive(HNObject *container) noexcept
{
    if (container && container->type() == HNObject::Topbar && TopbarModel::From(container) == m_activeSource)
        renderAllTopbars();
}

// --- Topbar menu opening --------------------------------------------------

void Firmament::setActiveTopbarItem(TopbarItem *item) noexcept
{
    if (item && !item->menu)
        return; // non-opening item (e.g. the app title)

    if (item && item->scene() && item->scene()->window())
    {
        if (activeTopbarItem.get() == item && m_activeMenu)
            return; // already open

        auto *tb { static_cast<TopbarSurface*>(item->scene()->window()) };
        closeControlPanel(); // A menu replaces it
        activeTopbarItem.reset(item);
        tb->setActiveItem(item);

        // Firmament owns two topbar-menu surfaces and alternates between them, reparenting the
        // activated menu's content into the free one for a smooth swap.
        if (!m_menuA) m_menuA = std::make_unique<MenuSurface>();
        if (!m_menuB) m_menuB = std::make_unique<MenuSurface>();

        MenuSurface *next { m_activeMenu.get() == m_menuA.get() ? m_menuB.get() : m_menuA.get() };

        m_prevMenu = m_activeMenu;
        m_activeMenu.reset(next);

        next->host(item->menu);
        next->beforeMap = [this]{
            if (m_prevMenu)
                m_prevMenu->setMapped(false);
        };

        next->openUnder(tb, item);
        return;
    }

    // Close.
    if (m_activeMenu)
        m_activeMenu->setMapped(false);

    hideTopbarOutline();
}

void Firmament::toggleControlPanel(TopbarSurface *topbar) noexcept
{
    if (controlPanel && controlPanel->mapped() && controlPanel->parent() == topbar)
    {
        closeControlPanel();
        return;
    }

    // Pressing the indicators while it's open: the compositor dismisses it (a press outside a popup)
    // before this press is handled here. That press closes it, it doesn't reopen it.
    if (controlPanel && controlPanel->parent() == topbar && CZTime::Ms() - m_controlPanelClosedMs < 250)
        return;

    // Replaces any open menu.
    setActiveTopbarItem(nullptr);

    if (!controlPanel)
    {
        controlPanel = std::make_unique<ControlPanel>();

        // Dismissed (clicking away, Escape...): drop the outline under the indicators, unless a
        // menu took its place.
        controlPanel->onMappedChanged.subscribe(this, [this]{
            if (!controlPanel->mapped())
                m_controlPanelClosedMs = CZTime::Ms();

            if (controlPanel->mapped() || m_activeMenu)
                return;

            for (MScreen *s : marco->screens())
                if (auto *tb { static_cast<TopbarSurface*>(s->userData) })
                    tb->setActiveItem(nullptr);
        });
    }

    if (controlPanel->mapped())
        controlPanel->setMapped(false);

    topbar->setActiveItem(&topbar->indicators);
    controlPanel->openUnder(topbar, &topbar->indicators);
}

void Firmament::closeControlPanel() noexcept
{
    if (controlPanel && controlPanel->mapped())
        controlPanel->setMapped(false);
}

void Firmament::onMenuUnmapped(MenuSurface *menu) noexcept
{
    // A topbar menu was dismissed (e.g. clicking away or pressing Escape) without another one
    // taking its place: drop the selection so the outline no longer sticks under its item.
    if (m_activeMenu.get() == menu)
        hideTopbarOutline();
}

void Firmament::hideTopbarOutline() noexcept
{
    activeTopbarItem.reset();
    m_activeMenu.reset();

    for (MScreen *s : marco->screens())
        if (auto *tb { static_cast<TopbarSurface*>(s->userData) })
            tb->setActiveItem(nullptr);
}

// --- Heaven wiring --------------------------------------------------------

void Firmament::initHeaven() noexcept
{
    heaven = HNBar::GetOrMake();
    if (!heaven)
        return;

    heaven->onObjectCreated.subscribe(this, [this](HNObject *o){ onObjectCreated(o); });
    heaven->onObjectDestroyed.subscribe(this, [this](HNObject *o){ onObjectDestroyed(o); });

    heaven->onActiveClientChanged.subscribe(this, [this](HNBar*){ updateActiveClient(); });

    // The unregistered app's actions arrived/changed. If its menu bar is being replaced while one
    // of its menus is open, close it first (the old bar is destroyed right after this).
    appActions->onChanged.subscribe(this, [this]{
        if (m_activeSource && m_activeSource != appActions->topbar() && m_activeSource != &nativeMenus->topbar
            && m_activeSource != &deskMenus->topbar && !m_activeSource->hnTopbar())
            setActiveTopbarItem(nullptr);
        updateActiveClient();
    });
    heaven->onClientTopbarChanged.subscribe(this, [this](HNClient *c){ if (c == heaven->activeClient()) updateActiveClient(); });
    heaven->onClientNameChanged.subscribe(this, [this](HNClient *c){ if (c == heaven->activeClient()) updateActiveClient(); });
    heaven->onClientDestroyed.subscribe(this, [this](HNClient*){ updateActiveClient(); });

    heaven->onObjectTitleChanged.subscribe(this, [](HNObject *o){
        if (o->type() == HNObject::Menu) { if (auto *m { MenuModel::From(o) }) m->setTitle(hnTitle(o)); }
        else if (auto *n { itemNode(o) }) n->setText(hnTitle(o));
    });
    heaven->onObjectIconChanged.subscribe(this, [](HNObject *o){
        if (o->type() == HNObject::Menu) { if (auto *m { MenuModel::From(o) }) m->setIconName(hnIcon(o)); }
        else if (auto *n { itemNode(o) }) n->setIconName(hnIcon(o));
    });
    heaven->onObjectShortcutChanged.subscribe(this, [](HNObject *o){
        if (auto *n { itemNode(o) }) n->setShortcut(hnShortcut(o));
    });
    heaven->onObjectEnabledChanged.subscribe(this, [](HNObject *o){
        if (o->type() == HNObject::Menu) { if (auto *m { MenuModel::From(o) }) m->setEnabled(hnEnabled(o)); }
        else if (auto *n { itemNode(o) }) n->setEnabled(hnEnabled(o));
    });
    heaven->onToggleCheckedChanged.subscribe(this, [](HNToggle *t){
        if (auto *n { itemNode(t) }) n->setChecked(t->checked());
    });

    heaven->onObjectParentChanged.subscribe(this, [this](HNObject *o){
        HNObject *parent { ParentOf(o) };
        if (!parent)
            detachObject(o);
        else
            insertChild(parent, o, nullptr); // appended
    });

    heaven->onObjectInsertedBefore.subscribe(this, [this](HNObject *o, HNObject *sibling){
        if (HNObject *parent { ParentOf(o) })
            insertChild(parent, o, sibling);
    });

    updateActiveClient();
}

void Firmament::onObjectCreated(HNObject *o) noexcept
{
    switch (o->type())
    {
    case HNObject::Topbar:
        new TopbarModel(static_cast<HNTopbar*>(o));
        break;
    case HNObject::Menu:
        new MenuModel(static_cast<HNMenu*>(o));
        break;
    case HNObject::Action:
    {
        auto *n { new Action(hnTitle(o)) };
        n->setIconName(hnIcon(o));
        n->setShortcut(hnShortcut(o));
        n->setEnabled(hnEnabled(o));
        n->onClicked = [o]{ o->click(); };
        o->userData = static_cast<MenuItem*>(n);
        break;
    }
    case HNObject::Toggle:
    {
        auto *n { new Toggle(hnTitle(o)) };
        n->setShortcut(hnShortcut(o));
        n->setChecked(static_cast<HNToggle*>(o)->checked());
        n->setEnabled(hnEnabled(o));
        n->onClicked = [o]{ o->click(); };
        o->userData = static_cast<MenuItem*>(n);
        break;
    }
    case HNObject::Divider:
    {
        auto *n { new Divider(hnTitle(o)) };
        o->userData = static_cast<MenuItem*>(n);
        break;
    }
    }
}

void Firmament::onObjectDestroyed(HNObject *o) noexcept
{
    switch (o->type())
    {
    case HNObject::Topbar:
        delete TopbarModel::From(o);
        break;
    case HNObject::Menu:
        delete MenuModel::From(o);
        break;
    default:
        delete itemNode(o);
        o->userData = nullptr;
        break;
    }
}

void Firmament::insertChild(HNObject *parent, HNObject *child, HNObject *before) noexcept
{
    if (parent->type() == HNObject::Topbar)
    {
        // The topbar's per-screen items are laid out by renderTopbar(), which reads the topbar's
        // children directly, so we only need to refresh if this topbar is the active one.
        reRenderIfActive(parent);
        return;
    }

    if (parent->type() != HNObject::Menu)
        return;

    auto *pm { MenuModel::From(parent) };
    if (!pm)
        return;

    AKNode *childNode { nodeFor(child) };
    if (!childNode)
        return;

    if (AKNode *beforeNode { before ? nodeFor(before) : nullptr })
        childNode->insertBefore(beforeNode);
    else
        childNode->setParent(pm->content());
}

void Firmament::detachObject(HNObject *o) noexcept
{
    if (o->type() == HNObject::Menu)
    {
        if (auto *m { MenuModel::From(o) })
            m->detach();
        renderAllTopbars(); // it may have been a top-level menu of the active bar
    }
    else if (auto *n { itemNode(o) })
        n->setParent(nullptr);
}

// Reads the process name of @p pid from /proc/<pid>/comm (falls back to "pid <n>").
static std::string ProcessName(UInt32 pid) noexcept
{
    std::ifstream file { "/proc/" + std::to_string(pid) + "/comm" };
    std::string name;
    std::getline(file, name);

    if (name.empty())
        name = "pid " + std::to_string(pid);

    return name;
}

// Name for an app that isn't a registered Heaven client. In order of preference: the name the
// compositor reported, the app id's .desktop entry name, the app id itself and, only if the
// compositor sent nothing else, the process name of its pid.
static std::string UnregisteredAppName(const HNBar &heaven) noexcept
{
    if (!heaven.activeClientAppName().empty())
        return heaven.activeClientAppName();

    if (const auto &appId { heaven.activeClientAppId() }; !appId.empty())
    {
        if (auto entry { GetFirm()->xdgkit->desktopEntry(appId) }; entry && !entry->name.empty())
            return entry->name;

        return appId;
    }

    if (const UInt32 pid { heaven.activeClientPid() }; pid != 0)
        return ProcessName(pid);

    return {};
}

void Firmament::updateActiveClient() noexcept
{
    std::string appName;
    TopbarModel *source { nullptr };
    bool nativeClient { false };
    std::string unregisteredAppId;

    auto *client { heaven->activeClient() };
    const bool unregistered { !client && (heaven->activeClientPid() != 0 ||
                                          !heaven->activeClientAppId().empty() ||
                                          !heaven->activeClientAppName().empty()) };

    if (unregistered)
        unregisteredAppId = heaven->activeClientAppId();

    // Switching away from (or to another) unregistered app drops the current action menu bar:
    // stop showing it before AppActions destroys it.
    if (unregisteredAppId != appActions->appId())
    {
        if (appActions->topbar() && m_activeSource == appActions->topbar())
        {
            setActiveTopbarItem(nullptr);
            m_activeSource = &nativeMenus->topbar;
        }

        appActions->setAppId(unregisteredAppId); // async: onChanged re-runs this when actions arrive
    }

    if (client)
    {
        // A Heaven client: use its name and its published menu bar.
        appName = client->name();
        source = TopbarModel::From(client->activeTopbar());
        if (!source)
        {
            nativeClient = true;
            source = &nativeMenus->topbar;
        }
    }
    else if (unregistered)
    {
        // An app that isn't a Heaven client (or hasn't registered yet), identified by the compositor.
        // Show the menus mapped from its D-Bus/.desktop actions, or the built-in "Window" menu. The
        // app menu switches to native mode.
        nativeClient = true;
        appName = UnregisteredAppName(*heaven);
        source = appActions->topbar() ? appActions->topbar() : &nativeMenus->topbar;
    }
    else
    {
        // Nothing active: show the built-in Desk defaults.
        appName = "Desk";
        source = &deskMenus->topbar;
    }

    setActiveSource(source);
    appMenu->setAppName(appName);
    appMenu->setNativeMode(nativeClient);

    for (MScreen *s : marco->screens())
        if (auto *tb { static_cast<TopbarSurface*>(s->userData) })
            tb->appTitle.title->setText(appName);
}
