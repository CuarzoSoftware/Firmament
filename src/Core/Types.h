#ifndef TYPES_H
#define TYPES_H

#define DEFAULT_ICON_THEME "WhiteSur-light"

constexpr float TOPBAR_HEIGHT { 24.f };
constexpr float TOPBAR_ICON_SIZE { 14.f };
constexpr float TOPBAR_FONT_SIZE { 12.f };
constexpr float TOPBAR_ITEM_PADDING { 10.f };
constexpr float TOPBAR_OUTLINE_PADDING { (TOPBAR_ITEM_PADDING + 1) * 0.5f };

constexpr float MENU_MIN_WIDTH { 200.f };
constexpr float MENU_MIN_HEIGHT { 24.f };
constexpr float MENU_VPADDING { 5.f };   // top/bottom padding of the item column
constexpr float MENU_HPADDING { 5.f };   // left/right padding of the item column
constexpr float MENU_ITEM_HEIGHT { 24.f };
constexpr float MENU_ITEM_HPADDING { 8.f };
constexpr float MENU_ITEM_RADIUS { 5.f };
constexpr float MENU_ICON_SIZE { 16.f };
constexpr float MENU_ITEM_GAP { 12.f };  // gap between title and the trailing shortcut/arrow
constexpr float MENU_DIVIDER_VMARGIN { 4.f };

constexpr float ITEM_FONT_SIZE { 12.f };


// Core
class Firmament;
class Assets;

// Surfaces — custom Marco surfaces.
class TopbarSurface;
class MenuSurface;

// Nodes — custom AK widgets.
class TopbarItem;
class MenuItem;
class MenuItemsContainer;
class Action;
class Toggle;
class Divider;
class SubMenu;

// Models — app-side models stored in HNObject::userData (only topbars and menus need one).
class TopbarModel;
class MenuModel;

// Menus — built-in (non-Heaven) menu definitions.
class MenuBuilder;
class SystemMenu;
class DeskMenus;
class AppMenu;
class NativeMenus;

Firmament *GetFirm() noexcept;

#endif // TYPES_H
