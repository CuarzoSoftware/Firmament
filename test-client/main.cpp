/**
 * Firmament test client.
 *
 * Publishes a small topbar (App / File / Edit / View) with some actions, a toggle and a divider,
 * so cz-firmament has a client to mirror in its global menu bar.
 */

#include <CZ/Heaven/Client/HNClient.h>
#include <CZ/Heaven/Client/HNTopbar.h>
#include <CZ/Heaven/Client/HNMenu.h>
#include <CZ/Heaven/Client/HNAction.h>
#include <CZ/Heaven/Client/HNToggle.h>
#include <CZ/Heaven/Client/HNDivider.h>
#include <CZ/Heaven/Client/HNLog.h>
#include <CZ/Core/CZCore.h>
#include <vector>

using namespace CZ;
using namespace CZ::HNClientAPI;

int main()
{
    setenv("CZ_HEAVEN_CLIENT_LOG_LEVEL", "6", 0);

    auto core { CZCore::GetOrMake() };
    auto client { HNClient::GetOrMake() };

    if (!client)
        return 1;

    std::vector<std::shared_ptr<HNObject>> keepAlive;

    auto topbar   { HNTopbar::Make() };
    auto fileMenu { HNMenu::Make("File", "", true, topbar.get()) };
    auto editMenu { HNMenu::Make("Edit", "", true, topbar.get()) };
    auto viewMenu { HNMenu::Make("View", "", true, topbar.get()) };

    /*

    auto openAction  { HNAction::Make("Open",  "document-open",    "Ctrl+O", true, fileMenu.get()) };
    auto saveAction  { HNAction::Make("Save",  "document-save",    "Ctrl+S", true, fileMenu.get()) };

    // Nested submenu: File -> Open Recent -> (files + a further submenu).
    auto recentMenu  { HNMenu::Make("Open Recent", "document-open-recent", true, fileMenu.get()) };
    auto recent1     { HNAction::Make("report.pdf",  "", "", true, recentMenu.get()) };
    auto recent2     { HNAction::Make("notes.txt",   "", "", true, recentMenu.get()) };
    auto recentDiv   { HNDivider::Make("", recentMenu.get()) };
    auto moreMenu    { HNMenu::Make("More", "", true, recentMenu.get()) };
    auto more1       { HNAction::Make("archive.zip", "", "", true, moreMenu.get()) };

    auto divider     { HNDivider::Make("", fileMenu.get()) };
    auto disabled    { HNAction::Make("Print", "document-print", "Ctrl+P", false, fileMenu.get()) };
    auto quitAction  { HNAction::Make("Quit",  "application-exit", "Ctrl+Q", true, fileMenu.get()) };

    auto copyAction  { HNAction::Make("Copy",  "edit-copy",  "Ctrl+C", true, editMenu.get()) };
    auto pasteAction { HNAction::Make("Paste", "edit-paste", "Ctrl+V", true, editMenu.get()) };
    auto editDiv     { HNDivider::Make("", editMenu.get()) };
    auto wrapToggle  { HNToggle::Make("Word Wrap", "", "Ctrl+W", true, true, editMenu.get()) };

    auto fullToggle  { HNToggle::Make("Full Screen", "", "F11", false, true, viewMenu.get()) };

    keepAlive = { topbar, fileMenu, editMenu, viewMenu,
                  openAction, saveAction, recentMenu, recent1, recent2, recentDiv, moreMenu, more1,
                  divider, disabled, quitAction,
                  copyAction, pasteAction, editDiv, wrapToggle, fullToggle };

    quitAction->onClicked.subscribe(quitAction.get(), [](HNObject*)
    {
        HNLog(CZInfo, "\"Quit\" clicked");
    });

    wrapToggle->onClicked.subscribe(wrapToggle.get(), [w = wrapToggle.get()](HNObject*)
    {
        w->setChecked(!w->checked());
        HNClient::Get()->commit();
    });*/

    client->setName("Firmament Demo");
    client->setActiveTopbar(topbar.get());
    client->setPrivateHandle("firmament-demo-client");
    client->commit();

    HNLog(CZInfo, "Test client running. Menu published.");

    while (core->dispatch() >= 0) {}

    return 0;
}
