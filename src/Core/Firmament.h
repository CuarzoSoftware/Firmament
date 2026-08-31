#ifndef FIRMAMENT_H
#define FIRMAMENT_H

#include <Core/Types.h>
#include <XDG/XDGKit.h>
#include <Marco/MApp.h>
#include <Heaven/Bar/HNBar.h>
#include <Ream/RCore.h>
#include <Ream/RImage.h>
#include <Surfaces/MenuSurface.h>
#include <Nodes/Action.h>

using namespace CZ;
using namespace CZ::HNBarAPI;

struct TestMenu
{
    TestMenu() noexcept{
        a4.setShortcut("⌥⇧⌘Esc");
        a8.setShortcut("⌃⌘Q");
        a9.setShortcut("⇧⌘Q");
        a9.setIcon("exit");
    }
    AKContainer systemMenu { YGFlexDirectionColumn };
    Action a1 { "Información del Sistema", &systemMenu };
    Action a2 { "Preferencias del Sistema...", &systemMenu };
    Action a3 { "Tienda de Software", &systemMenu };
    Action a4 { "Forzar salida de Desk", &systemMenu };
    Action a5 { "Reposo", &systemMenu };
    Action a6 { "Reiniciar...", &systemMenu };
    Action a7 { "Apagar equipo...", &systemMenu };
    Action a8 { "Bloquear pantalla", &systemMenu };
    Action a9 { "Cerrar sesión", &systemMenu };
};

class Firmament final : public CZObject
{
public:
    Firmament() noexcept;
    std::shared_ptr<MApp> marco;
    std::shared_ptr<XDGKit> xdgkit;
    std::shared_ptr<HNBar> heaven;
    std::shared_ptr<RImage> logo;
    std::shared_ptr<MenuSurface> menu, prevMenu;
    std::shared_ptr<TestMenu> testMenu;

    RDRMFormat safeFormat { 0 , {}};

    void setActiveTopbarItem(TopbarItem *item = nullptr) noexcept;
    CZWeak<TopbarItem> activeTopbarItem;

    void setActiveMenuItem(MenuItem *item = nullptr) noexcept;
    CZWeak<MenuItem> activeMenuItem;

    std::shared_ptr<AKRoundSolidColor> menuItemOutline;

    void updateActiveClient() noexcept;
private:
    void initScreens() noexcept;
    void initHeaven() noexcept;
};

#endif // FIRMAMENT_H
