#include <Apps/AppActions.h>
#include <Core/Firmament.h>
#include <Core/Log.h>
#include <Menus/NativeMenus.h>
#include <Models/MenuModel.h>
#include <Nodes/Action.h>
#include <Nodes/Divider.h>
#include <Nodes/MenuItemsContainer.h>
#include <CZ/Core/CZBus.h>
#include <systemd/sd-bus.h>
#include <algorithm>
#include <cstring>

using namespace CZ;

static constexpr const char *kGtkActionsIface { "org.gtk.Actions" };
static constexpr const char *kFdoAppIface { "org.freedesktop.Application" };

// Well-known parameterless GTK actions mapped to the menu bar.
struct MappedAction
{
    const char *name;
    const char *title;
    const char *shortcut;
};

static constexpr MappedAction kFileActions[]
{
    { "new-window",   "New Window",   "⌘N" },
    { "new-document", "New Document", "" },
    { "open",         "Open…",        "⌘O" },
};

static constexpr MappedAction kHelpActions[]
{
    { "help",      "Help",               "" },
    { "shortcuts", "Keyboard Shortcuts", "" },
};

struct AppActions::Menus final : public MenuBuilder
{
    Menus(AppActions *actions) noexcept
    {
        MenuModel *file { addGtkActions(actions, "File", kFileActions) };

        // .desktop actions not already exported as GTK actions (D-Bus activatable GApplications
        // usually export the same ids).
        bool dividerAdded { false };

        if (const auto &entry { actions->desktopEntry() })
        {
            for (const auto &desktopAction : entry->actions)
            {
                if (actions->has(desktopAction.id))
                    continue;

                if (!file)
                    file = newMenu("File");
                else if (!dividerAdded && !file->content()->children().empty())
                    addDivider(file);

                dividerAdded = true;
                auto *item { addAction(file, desktopAction.name) };
                item->onClicked = [actions, id = desktopAction.id]{ actions->activateDesktopAction(id); };
            }
        }

        if (file)
            topbar.insertMenu(file, nullptr);

        // Shared with NativeMenus (only one menu bar is shown at a time).
        topbar.insertMenu(GetFirm()->nativeMenus->windowMenu(), nullptr);

        MenuModel *help { addGtkActions(actions, "Help", kHelpActions) };

        if (help)
            topbar.insertMenu(help, nullptr);

        empty = !file && !help;
    }

    template<size_t N>
    MenuModel *addGtkActions(AppActions *actions, const char *title, const MappedAction (&table)[N]) noexcept
    {
        MenuModel *menu {};

        for (const auto &mapped : table)
        {
            if (!actions->has(mapped.name))
                continue;

            if (!menu)
                menu = newMenu(title);

            auto *item { addAction(menu, mapped.title, mapped.shortcut) };
            item->setEnabled(actions->enabled(mapped.name));
            item->onClicked = [actions, name = std::string(mapped.name)]{ actions->activate(name); };
            gtkItems[mapped.name] = item;
        }

        return menu;
    }

    TopbarModel topbar {};
    std::unordered_map<std::string, Action*> gtkItems;
    bool empty { true };
};

AppActions::AppActions() noexcept {}

AppActions::~AppActions() noexcept
{
    sd_bus_slot_unref(m_querySlot);
    sd_bus_slot_unref(m_changedSlot);
}

bool AppActions::IsValidBusName(const std::string &name) noexcept
{
    // See the D-Bus specification, "Valid Bus Names" (well-known names only).
    if (name.empty() || name.size() > 255 || name.front() == '.' || name.back() == '.')
        return false;

    size_t elements { 1 };
    bool elementStart { true };

    for (const char c : name)
    {
        if (c == '.')
        {
            if (elementStart)
                return false; // empty element
            elements++;
            elementStart = true;
            continue;
        }

        const bool alpha { (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-' };
        const bool digit { c >= '0' && c <= '9' };

        if (!alpha && !(digit && !elementStart))
            return false;

        elementStart = false;
    }

    return elements >= 2;
}

TopbarModel *AppActions::topbar() const noexcept
{
    return m_menus && !m_menus->empty ? &m_menus->topbar : nullptr;
}

bool AppActions::enabled(const std::string &name) const noexcept
{
    auto it { m_gtkActions.find(name) };
    return it != m_gtkActions.end() && it->second;
}

void AppActions::setAppId(const std::string &appId) noexcept
{
    if (appId == m_appId)
        return;

    reset();
    m_appId = appId;

    if (m_appId.empty())
        return;

    m_entry = GetFirm()->xdgkit->desktopEntry(m_appId);

    // GApplications own their app id; D-Bus activatable apps own their desktop file id (usually
    // the same, but the .desktop lookup may have matched a lowercase variant).
    if (IsValidBusName(m_appId))
        m_busName = m_appId;
    else if (m_entry && IsValidBusName(m_entry->id))
        m_busName = m_entry->id;

    Log(CZDebug, CZLN, "[AppActions] Tracking app id {} (desktop entry: {}, bus name: {})", m_appId,
        m_entry ? m_entry->path.string() : "none", m_busName.empty() ? "none" : m_busName);

    // .desktop actions are known right away (the caller reads topbar() after this, so no signal).
    rebuild(false);

    if (m_busName.empty())
        return;

    // Object path: "/" + bus name with '.' -> '/' and '-' -> '_'.
    m_path = "/" + m_busName;
    std::replace(m_path.begin(), m_path.end(), '.', '/');
    std::replace(m_path.begin(), m_path.end(), '-', '_');

    auto bus { CZBus::GetOrMakeUser() };

    if (!bus)
        return;

    // Re-query whenever the app's actions change (added/removed/enabled toggled).
    const int r { sd_bus_match_signal_async(bus->bus(), &m_changedSlot, m_busName.c_str(), m_path.c_str(),
                                            kGtkActionsIface, "Changed", &ChangedSignal, nullptr, this) };

    if (r < 0)
        Log(CZWarning, CZLN, "[AppActions] Failed to watch {} actions: {}", m_busName, strerror(-r));

    query();
}

void AppActions::reset() noexcept
{
    m_querySlot = sd_bus_slot_unref(m_querySlot);
    m_changedSlot = sd_bus_slot_unref(m_changedSlot);
    m_appId.clear();
    m_busName.clear();
    m_path.clear();
    m_entry.reset();
    m_gtkActions.clear();

    // The caller (Firmament) stops showing our menu bar before switching apps, so it's safe to drop
    // it here without notifying.
    m_menus.reset();
}

void AppActions::query() noexcept
{
    auto bus { CZBus::GetOrMakeUser() };

    if (!bus || m_busName.empty())
        return;

    m_querySlot = sd_bus_slot_unref(m_querySlot);

    sd_bus_message *msg {};
    int r { sd_bus_message_new_method_call(bus->bus(), &msg, m_busName.c_str(), m_path.c_str(), kGtkActionsIface, "DescribeAll") };

    if (r >= 0)
    {
        // Never launch a D-Bus activatable app just because one of its windows got focus.
        sd_bus_message_set_auto_start(msg, 0);
        r = sd_bus_call_async(bus->bus(), &m_querySlot, msg, &DescribeAllReply, this, 0);
    }

    sd_bus_message_unref(msg);

    if (r < 0)
        Log(CZDebug, CZLN, "[AppActions] DescribeAll({}) failed: {}", m_busName, strerror(-r));
}

int AppActions::DescribeAllReply(sd_bus_message *m, void *userdata, sd_bus_error *)
{
    auto *self { static_cast<AppActions*>(userdata) };
    self->m_querySlot = sd_bus_slot_unref(self->m_querySlot);

    std::unordered_map<std::string, bool> actions;

    if (sd_bus_message_is_method_error(m, nullptr))
    {
        // Most apps don't export org.gtk.Actions (or don't own their app id): no GTK actions.
        const sd_bus_error *error { sd_bus_message_get_error(m) };
        Log(CZDebug, CZLN, "[AppActions] {} exports no GTK actions: {}", self->m_busName,
            error && error->message ? error->message : "unknown error");
        self->update(std::move(actions));
        return 0;
    }

    // a{s(bgav)}: name -> (enabled, parameter type, state)
    int r { sd_bus_message_enter_container(m, 'a', "{s(bgav)}") };

    while (r >= 0 && (r = sd_bus_message_enter_container(m, 'e', "s(bgav)")) > 0)
    {
        const char *name {}, *paramType {};
        int enabled {};

        if ((r = sd_bus_message_read(m, "s", &name)) < 0 ||
            (r = sd_bus_message_enter_container(m, 'r', "bgav")) < 0 ||
            (r = sd_bus_message_read(m, "bg", &enabled, &paramType)) < 0 ||
            (r = sd_bus_message_skip(m, "av")) < 0 ||
            (r = sd_bus_message_exit_container(m)) < 0 ||
            (r = sd_bus_message_exit_container(m)) < 0)
            break;

        if (paramType[0] == '\0')
            actions[name] = enabled != 0;
    }

    if (r < 0)
    {
        Log(CZWarning, CZLN, "[AppActions] Invalid DescribeAll reply from {}: {}", self->m_busName, strerror(-r));
        actions.clear();
    }
    else
        Log(CZDebug, CZLN, "[AppActions] {} exports {} parameterless GTK actions", self->m_busName, actions.size());

    self->update(std::move(actions));
    return 0;
}

int AppActions::ChangedSignal(sd_bus_message *, void *userdata, sd_bus_error *)
{
    static_cast<AppActions*>(userdata)->query();
    return 0;
}

void AppActions::update(std::unordered_map<std::string, bool> &&actions) noexcept
{
    if (actions == m_gtkActions)
        return;

    bool sameKeys { actions.size() == m_gtkActions.size() };

    for (auto it = actions.begin(); sameKeys && it != actions.end(); it++)
        sameKeys = m_gtkActions.contains(it->first);

    m_gtkActions = std::move(actions);

    if (sameKeys)
    {
        // Same actions, only enabled states changed: update in place.
        if (m_menus)
            for (auto &[name, item] : m_menus->gtkItems)
                item->setEnabled(enabled(name));

        onChanged.notify();
        return;
    }

    rebuild();
}

void AppActions::rebuild(bool notify) noexcept
{
    // Keep the old bar alive until listeners have switched to the new one.
    auto old { std::move(m_menus) };
    m_menus = std::make_unique<Menus>(this);

    if (notify)
        onChanged.notify();
}

void AppActions::activate(const std::string &name) noexcept
{
    auto bus { CZBus::GetOrMakeUser() };

    if (!bus || m_busName.empty())
        return;

    Log(CZDebug, CZLN, "[AppActions] org.gtk.Actions.Activate({}) on {}", name, m_busName);

    // org.gtk.Actions.Activate(s action, av parameter, a{sv} platform_data)
    const int r { sd_bus_call_method_async(bus->bus(), nullptr, m_busName.c_str(), m_path.c_str(), kGtkActionsIface,
                                           "Activate", nullptr, nullptr, "sava{sv}", name.c_str(), 0, 0) };

    if (r < 0)
        Log(CZWarning, CZLN, "[AppActions] Failed to activate {} on {}: {}", name, m_busName, strerror(-r));
}

void AppActions::activateDesktopAction(const std::string &id) noexcept
{
    if (!m_entry)
        return;

    auto it { std::find_if(m_entry->actions.begin(), m_entry->actions.end(), [&id](const auto &a){ return a.id == id; }) };

    if (it == m_entry->actions.end())
        return;

    // Desktop Entry spec: D-Bus activatable apps are activated via org.freedesktop.Application,
    // owned by the desktop file id (here, auto-starting the app is intended).
    if (m_entry->dbusActivatable && IsValidBusName(m_entry->id))
    {
        if (auto bus { CZBus::GetOrMakeUser() })
        {
            std::string path { "/" + m_entry->id };
            std::replace(path.begin(), path.end(), '.', '/');
            std::replace(path.begin(), path.end(), '-', '_');

            Log(CZDebug, CZLN, "[AppActions] org.freedesktop.Application.ActivateAction({}) on {}", id, m_entry->id);

            // ActivateAction(s action_name, av parameter, a{sv} platform_data)
            const int r { sd_bus_call_method_async(bus->bus(), nullptr, m_entry->id.c_str(), path.c_str(), kFdoAppIface,
                                                   "ActivateAction", nullptr, nullptr, "sava{sv}", id.c_str(), 0, 0) };
            if (r >= 0)
                return;

            Log(CZWarning, CZLN, "[AppActions] ActivateAction({}) on {} failed: {}", id, m_entry->id, strerror(-r));
        }
    }

    if (!it->exec.empty() && !m_entry->launch(it->exec))
        Log(CZWarning, CZLN, "[AppActions] Failed to launch {} action {}", m_entry->id, id);
}
