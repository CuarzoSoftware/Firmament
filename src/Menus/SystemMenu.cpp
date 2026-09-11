#include <Menus/SystemMenu.h>
#include <Models/MenuModel.h>
#include <Nodes/MenuItem.h>

SystemMenu::SystemMenu() noexcept
{
    m_menu = newMenu();

    addAction(m_menu, "Información del Sistema");

    auto *apps { addSubMenu(m_menu, "Aplicaciones") };
    addAction(apps, "Terminal");
    addAction(apps, "Editor de Texto");
    addAction(apps, "Navegador");

    addDivider(m_menu);
    addAction(m_menu, "Preferencias del Sistema...");
    addAction(m_menu, "Tienda de Software");

    addDivider(m_menu);
    addAction(m_menu, "Forzar salida de Desk", "⌥⇧⌘Esc");

    addDivider(m_menu);
    addAction(m_menu, "Reposo");
    addAction(m_menu, "Reiniciar...");
    addAction(m_menu, "Apagar equipo...");

    addDivider(m_menu);
    addAction(m_menu, "Bloquear pantalla", "⌃⌘Q");
    addAction(m_menu, "Cerrar sesión", "⇧⌘Q");
}

