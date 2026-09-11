#ifndef DESKMENUS_H
#define DESKMENUS_H

#include <Menus/MenuBuilder.h>
#include <Models/TopbarModel.h>

/**
 * @brief The default menu bar shown when no client is active, as if an app called "Desk" were.
 *
 * Example menus/items the user can freely edit.
 */
class DeskMenus final : public MenuBuilder
{
public:
    DeskMenus() noexcept;

    TopbarModel topbar {}; ///< The standalone menu bar (its menus live in the MenuBuilder base).
};

#endif // DESKMENUS_H
