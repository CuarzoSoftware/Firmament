#ifndef FIRMAMENT_H
#define FIRMAMENT_H

#include <Core/Types.h>
#include <Core/Assets.h>
#include <XDG/XDGKit.h>
#include <Marco/MApp.h>
#include <Heaven/Bar/HNBar.h>
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

    /// Opens @p item's popup under it (nullptr closes the open topbar menu).
    void setActiveTopbarItem(TopbarItem *item = nullptr) noexcept;
    CZWeak<TopbarItem> activeTopbarItem;

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

    void initScreens() noexcept;
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
