#include <Ream/RDevice.h>
#include <Marco/MLog.h>
#include <Core/Firmament.h>
#include <Core/CZTimer.h>
#include <Surfaces/TopbarSurface.h>
#include <Models/TopbarModel.h>
#include <Marco/MScreen.h>
#include <Heaven/Bar/HNClient.h>
#include <Heaven/Bar/HNTopbar.h>
#include <Heaven/Bar/HNMenu.h>
#include <Heaven/Bar/HNWithParent.h>

static Firmament *_firm;

Firmament *GetFirm() noexcept
{
    return _firm;
}

Firmament::Firmament() noexcept
{
    _firm = this;
    marco = MApp::Make();

    if (!marco)
        return;

    xdgkit = XDGKit::Make();

    if (!xdgkit)
        return;

    menuItemOutline = std::make_shared<AKRoundSolidColor>(AKTheme::SystemBlue.light(), CZBorderRadius::Make(4));
    menuItemOutline->layout().setPositionType(YGPositionTypeAbsolute);
    menuItemOutline->layout().setWidthPercent(100.f);

    auto ream { RCore::Get() };
    auto fmt { ream->mainDevice()->textureFormats().formats().find(DRM_FORMAT_ABGR8888) };
    if (fmt == ream->mainDevice()->textureFormats().formats().end())
        return;

    safeFormat = *fmt;
    testMenu = std::make_shared<TestMenu>();
    logo = RImage::LoadFile("/home/eduardo/Cuarzo.png", safeFormat);

    initScreens();
    initHeaven();
}

void Firmament::setActiveTopbarItem(TopbarItem *item) noexcept
{
    if (item && item->scene() && item->scene()->window())
    {
        activeTopbarItem.reset(item);
        auto *tb { static_cast<TopbarSurface*>(item->scene()->window()) };
        tb->setActiveItem(item);

        if (menu)
            prevMenu = menu;

        menu.reset(new MenuSurface());
        menu->setParent(tb);
        menu->setAnchor(MPopup::Anchor::BottomLeft);
        menu->setAnchorRect(SkIRect::MakeXYWH(
            item->worldRect().x() - TOPBAR_OUTLINE_PADDING,
            0,
            item->worldRect().width(),
            TOPBAR_HEIGHT));
        menu->onWillMap.subscribe(menu.get(),[this](){
            if (prevMenu)
                prevMenu->setMapped(false);
        });
        testMenu->systemMenu.setParent(&menu->content);
        auto minSize { menu->minContentSize() };
        menu->layout().setWidth(minSize.width());
        menu->layout().setHeight(minSize.height());
        menu->setMapped(true);

        return;
    }

    activeTopbarItem.reset();

    for (MScreen *s : marco->screens())
    {
        auto *tb { static_cast<TopbarSurface*>(s->userData) };
        tb->setActiveItem(nullptr);
    }
}

void Firmament::setActiveMenuItem(MenuItem *item) noexcept
{
    if (item == activeMenuItem) return;

    if (activeMenuItem)
        activeMenuItem->setHover(false);

    activeMenuItem = item;

    if (activeMenuItem)
    {
        activeMenuItem->setHover(true);
        menuItemOutline->insertBefore(activeMenuItem);
        menuItemOutline->layout().setHeight(activeMenuItem->worldRect().height());
        menuItemOutline->layout().setPosition(YGEdgeLeft, 0.f);
        menuItemOutline->layout().setPosition(YGEdgeTop, activeMenuItem->layout().calculatedTop());
    }
    else
    {
        menuItemOutline->setParent(nullptr);
    }
}

void Firmament::initScreens() noexcept
{
    marco->onScreenPlugged.subscribe(this, [](MScreen &s) { new TopbarSurface(s); });
    marco->onScreenUnplugged.subscribe(this, [](MScreen &s){ delete static_cast<TopbarSurface*>(s.userData); });

    for (MScreen *s : marco->screens())
        new TopbarSurface(*s);
}

void Firmament::initHeaven() noexcept
{
    heaven = HNBar::GetOrMake();

    if (!heaven)
        return;

    heaven->onObjectCreated.subscribe(this, [](HNObject *o) {
        switch (o->type())
        {
        //case HNObject::Topbar:  new TopbarModel(static_cast<HNTopbar*>(o)); break;
        //case HNObject::Menu:    new MenuModel(static_cast<HNMenu*>(o)); break;
        //case HNObject::Action:  new ActionModel(static_cast<HNAction*>(o)); break;
        //case HNObject::Toggle:  new ToggleModel(static_cast<HNToggle*>(o)); break;
        //case HNObject::Divider: new DividerModel(static_cast<HNDivider*>(o)); break;
        default: break;
        }
    });

    heaven->onObjectDestroyed.subscribe(this, [](HNObject *o) {
        /*
        if (o->userData)
        {
            delete static_cast<ObjectModel*>(o->userData);
            o->userData = nullptr;
        }*/
    });

    // Active client / its active topbar / its name → swap the row shown in every topbar.
    heaven->onActiveClientChanged.subscribe(this, [this](HNBar*){ updateActiveClient(); });
    // heaven->onClientTopbarChanged.subscribe(this, [this](HNClient *c){ if (c == heaven->activeClient()) updateActiveClient(); });
    heaven->onClientNameChanged.subscribe(this, [this](HNClient *c){ updateActiveClient(); });

    /*
    // Property changes → the object's model re-reads and updates all of its nodes.
    heaven->onObjectTitleChanged.subscribe(this, [](HNObject *o){ if (auto *m { ObjectModel::From(o) }) m->refreshTitle(); });
    heaven->onObjectIconChanged.subscribe(this, [](HNObject *o){ if (auto *m { ObjectModel::From(o) }) m->refreshIcon(); });
    heaven->onObjectShortcutChanged.subscribe(this, [](HNObject *o){ if (auto *m { ObjectModel::From(o) }) m->refreshShortcut(); });
    heaven->onObjectEnabledChanged.subscribe(this, [](HNObject *o){ if (auto *m { ObjectModel::From(o) }) m->refreshEnabled(); });
    heaven->onToggleCheckedChanged.subscribe(this, [](HNToggle *t){ if (auto *m { ObjectModel::From(t) }) m->refreshChecked(); });

    // Parent changed: appended to a container, or detached (parent == nullptr).
    heaven->onObjectParentChanged.subscribe(this, [](HNObject *o){
        HNObject *parent { ParentOf(o) };
        if (!parent)
        {
            if (auto *m { ObjectModel::From(o) })
                m->detach();
            return;
        }

        insertChild(parent, o, nullptr); // appended to the end
    });

    // Reordered before a sibling within its container.
    heaven->onObjectInsertedBefore.subscribe(this, [](HNObject *o, HNObject *sibling){
        if (HNObject *parent { ParentOf(o) })
            insertChild(parent, o, sibling);
    });

    updateActiveClient();*/
}

void Firmament::updateActiveClient() noexcept
{
    std::string appName { "Desk" };

    auto *activeClient { heaven->activeClient() };

    if (!activeClient && !heaven->clients().empty())
        activeClient = heaven->clients().begin()->second.get();

    if (activeClient)
        appName = activeClient->name();

    for (MScreen *s : marco->screens())
    {
        auto *tb { static_cast<TopbarSurface*>(s->userData) };
        tb->appTitle.title->setText(appName);
    }
}
