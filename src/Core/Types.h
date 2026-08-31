#ifndef TYPES_H
#define TYPES_H

#define DEFAULT_ICON_THEME "WhiteSur-light"

constexpr float TOPBAR_HEIGHT { 24.f };
constexpr float TOPBAR_ICON_SIZE { 14.f };
constexpr float TOPBAR_FONT_SIZE { 12.f };
constexpr float TOPBAR_ITEM_PADDING { 10.f };
constexpr float TOPBAR_OUTLINE_PADDING { (TOPBAR_ITEM_PADDING + 1) * 0.5f };

constexpr float MENU_MIN_WIDTH { 200.f };
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
class MenuController;

// Surfaces (Surfaces/*Surface) — custom Marco surfaces.
class TopbarSurface;
class MenuSurface;

// Nodes (Nodes/*Node) — custom AK widgets.
class TopbarItem;
class Action;
class MenuItemNode;
class DividerNode;

// Models (Models/*Model) — app-side mirrors of Heaven objects, stored in HNObject::userData.
class ObjectModel;
class TopbarModel;
class MenuModel;
class ActionModel;
class ToggleModel;
class DividerModel;

Firmament *GetFirm() noexcept;

#endif // TYPES_H
