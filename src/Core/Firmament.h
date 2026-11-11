#ifndef FIRMAMENT_H
#define FIRMAMENT_H

#include <Core/Types.h>
#include <Core/Assets.h>
#include <XDG/XDGKit.h>
#include <Marco/MApp.h>
#include <Heaven/Bar/HNBar.h>
#include <Core/CZTimer.h>
#include <memory>

using namespace CZ;
using namespace CZ::HNBarAPI;

/**
 * @brief The Firmament global menu app.
 *
 * Owns the Marco app, the XDG kit, the Heaven bar and the shared assets, and drives the whole UI:
 *  - one TopbarSurface per screen,
 *  - a model layer (TopbarModel/MenuModel) mirroring the Heaven objects,
 *  - built-in "Desk" and system menus so the bar is fully functional with no client connected.
 *
 * The topbar shows an "active source" (a TopbarModel): the active client's menu bar, or the Desk
 * defaults when no client is active.
 */
class Firmament final : public CZObject
{
public:
    Firmament() noexcept;
    ~Firmament() noexcept;

    std::shared_ptr<MApp> marco;
    std::shared_ptr<XDGKit> xdgkit;
    std::shared_ptr<HNBar> heaven;

    Assets assets;
    std::unique_ptr<SystemMenu> systemMenu;
    std::unique_ptr<DeskMenus> deskMenus;
    std::unique_ptr<AppMenu> appMenu;
    std::unique_ptr<NativeMenus> nativeMenus;

    /// GTK (org.gtk.Actions) and .desktop actions of the active app when it isn't a registered
    /// Heaven client (tracked from the app id reported by the compositor).
    std::unique_ptr<AppActions> appActions;

    /// Opens @p item's popup under it (nullptr closes the open topbar menu).
    void setActiveTopbarItem(TopbarItem *item = nullptr) noexcept;
    CZWeak<TopbarItem> activeTopbarItem;

    /// Size of the bar and its menus on every screen (1 = normal, see TOPBAR_SCALES).
    float uiScale() const noexcept { return m_uiScale; }

    /// Resizes the bar and menus on every screen and saves the choice in
    /// `$XDG_CONFIG_HOME/cz-firmament/firmament.conf` (read by Desk too, to keep its icons clear of
    /// the bar).
    void setUIScale(float scale) noexcept;

    /// The size menu at the right end of the bar.
    std::unique_ptr<ScaleMenu> scaleMenu;

    /// Network, volume and battery shown by the bars' indicators.
    std::unique_ptr<Status> status;

    /// Opens the control panel under @p topbar's indicators, or closes it if already open there.
    void toggleControlPanel(TopbarSurface *topbar) noexcept;
    void closeControlPanel() noexcept;
    std::unique_ptr<ControlPanel> controlPanel;
    UInt32 m_controlPanelClosedMs { 0 }; // When it was last hidden

    /// Radios, lights, appearance and session actions (the control panel's controls).
    std::unique_ptr<Controls> controls;

    /**
     * @brief The top bar clock's text, in the user's locale (LC_TIME), e.g. "Mar 10 nov. 11:52" in
     *        Spanish or "Tue Nov 10 11:52" in English.
     */
    std::string clockText() const noexcept;

    /// Refreshes the clock on every top bar.
    void updateClock() noexcept;

    /// Fills @p surface's menusArea with the active source's per-screen topbar items.
    void renderTopbar(TopbarSurface *surface) noexcept;

    /// Logs (CZDebug) the current client name (or the built-in topbar name) and its whole
    /// topbar -> menus -> items tree as an indented hierarchy. Debugging aid.
    void logMenuHierarchy() noexcept;

    /// Called by a MenuSurface when it unmaps. If it was the active topbar menu (and nothing is
    /// replacing it), the topbar selection outline is hidden across all screens.
    void onMenuUnmapped(MenuSurface *menu) noexcept;

private:
    void hideTopbarOutline() noexcept;
    void scheduleClock() noexcept;
    void loadSettings() noexcept;
    void saveSettings() noexcept;
    float m_uiScale { 1.f };
    std::unique_ptr<CZTimer> m_clockTimer;

    void initScreens() noexcept;
    void onScreenPlugged(MScreen &screen) noexcept;
    void onScreenUnplugged(MScreen &screen) noexcept;
    void initHeaven() noexcept;

    void setActiveSource(TopbarModel *source) noexcept;
    void renderAllTopbars() noexcept;
    void reRenderIfActive(HNObject *container) noexcept;
    void updateActiveClient() noexcept;

    void onObjectCreated(HNObject *o) noexcept;
    void onObjectDestroyed(HNObject *o) noexcept;
    void insertChild(HNObject *parent, HNObject *child, HNObject *before) noexcept;
    void detachObject(HNObject *o) noexcept;

    TopbarModel *m_activeSource {};

    // The two pooled topbar-menu surfaces (lazy), and which is open / being replaced. A menu's
    // content is reparented into one of these when its topbar item is activated.
    std::unique_ptr<MenuSurface> m_menuA, m_menuB;
    CZWeak<MenuSurface> m_activeMenu, m_prevMenu;
};

#endif // FIRMAMENT_H
