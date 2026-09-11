#include <Menus/DeskMenus.h>
#include <Models/MenuModel.h>
#include <Nodes/MenuItem.h>

DeskMenus::DeskMenus() noexcept
{
    // Archivo
    auto *archivo { newMenu("Archivo") };
    topbar.insertMenu(archivo, nullptr);
    addAction(archivo, "Nuevo", "⌘N");
    addAction(archivo, "Abrir…", "⌘O");
    auto *recientes { addSubMenu(archivo, "Abrir Reciente") };
    addAction(recientes, "documento.txt");
    addAction(recientes, "informe.pdf");
    addDivider(recientes);
    addAction(recientes, "Vaciar Menú");
    addDivider(archivo);
    addAction(archivo, "Guardar", "⌘S");
    addAction(archivo, "Cerrar", "⌘W");

    // Edición
    auto *edicion { newMenu("Edición") };
    topbar.insertMenu(edicion, nullptr);
    addAction(edicion, "Deshacer", "⌘Z");
    addAction(edicion, "Rehacer", "⇧⌘Z");
    addDivider(edicion);
    addAction(edicion, "Cortar", "⌘X");
    addAction(edicion, "Copiar", "⌘C");
    addAction(edicion, "Pegar", "⌘V");

    // Visualización
    auto *ver { newMenu("Visualización") };
    topbar.insertMenu(ver, nullptr);
    addToggle(ver, "Mostrar Barra Lateral", true);
    addToggle(ver, "Mostrar Barra de Estado");
    addDivider(ver);
    addAction(ver, "Pantalla Completa", "⌃⌘F");

    // Ir
    auto *ir { newMenu("Ir") };
    topbar.insertMenu(ir, nullptr);
    addAction(ir, "Atrás", "⌘[");
    addAction(ir, "Adelante", "⌘]");

    // Ventana
    auto *ventana { newMenu("Ventana") };
    topbar.insertMenu(ventana, nullptr);
    addAction(ventana, "Minimizar", "⌘M");
    addAction(ventana, "Zoom");

    // Ayuda
    auto *ayuda { newMenu("Ayuda") };
    topbar.insertMenu(ayuda, nullptr);
    addAction(ayuda, "Ayuda de Desk");
}
