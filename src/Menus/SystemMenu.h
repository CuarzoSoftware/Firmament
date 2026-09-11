#ifndef SYSTEMMENU_H
#define SYSTEMMENU_H

#include <Menus/MenuBuilder.h>

/**
 * @brief The menu opened by the system (Cuarzo) logo at the far left of every topbar.
 *
 * Example content the user can freely edit.
 */
class SystemMenu final : public MenuBuilder
{
public:
    SystemMenu() noexcept;

    MenuModel *menu() const noexcept { return m_menu; }

private:
    MenuModel *m_menu {};
};

#endif // SYSTEMMENU_H
