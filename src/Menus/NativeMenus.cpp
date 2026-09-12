#include <Menus/NativeMenus.h>
#include <Models/MenuModel.h>
#include <Nodes/Action.h>
#include <Core/Firmament.h>
#include <Heaven/Bar/HNBar.h>

using namespace CZ;

NativeMenus::NativeMenus() noexcept
{
    auto *window { newMenu("Window") };
    topbar.insertMenu(window, nullptr);

    auto *minimized  { addAction(window, "Toggle Minimized") };
    auto *maximized  { addAction(window, "Toggle Maximized") };
    auto *fullscreen { addAction(window, "Toggle Fullscreen") };
    auto *close      { addAction(window, "Close") };

    // Each is a window-management action the compositor performs on the active client.
    minimized->onClicked  = []{ GetFirm()->heaven->toggleActiveClientMinimized(); };
    maximized->onClicked  = []{ GetFirm()->heaven->toggleActiveClientMaximized(); };
    fullscreen->onClicked = []{ GetFirm()->heaven->toggleActiveClientFullscreen(); };
    close->onClicked      = []{ GetFirm()->heaven->closeActiveClient(); };
}
