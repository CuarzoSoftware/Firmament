#ifndef NATIVEMENUS_H
#define NATIVEMENUS_H

#include <Menus/MenuBuilder.h>
#include <Models/TopbarModel.h>

/**
 * @brief The menu bar shown when a native (non-Heaven) app is active.
 *
 * Such apps publish no menus of their own, so Firmament shows a single "Window" menu whose actions
 * are forwarded to the compositor (toggle minimized/maximized/fullscreen, close).
 */
class NativeMenus final : public MenuBuilder
{
public:
    NativeMenus() noexcept;

    TopbarModel topbar {}; ///< The standalone menu bar (its menus live in the MenuBuilder base).
};

#endif // NATIVEMENUS_H
