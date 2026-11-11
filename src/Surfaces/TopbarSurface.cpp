#include <Marco/MScreen.h>
#include <cmath>
#include <Surfaces/TopbarSurface.h>
#include <Nodes/TopbarItem.h>
#include <Core/Firmament.h>
#include <Core/Assets.h>
#include <Core/Status.h>
#include <Menus/SystemMenu.h>
#include <Menus/AppMenu.h>
#include <Menus/ScaleMenu.h>
#include <AK/AKTheme.h>
#include <AK/Nodes/AKImageFrame.h>

TopbarSurface::TopbarSurface(MScreen &screen) noexcept
{
    screen.userData = this;
    setScreen(&screen);

    logo.setIcon(GetFirm()->assets.logo);
    logo.menu = GetFirm()->systemMenu->menu();

    // The logo keeps its own colors in light mode, white in dark mode.
    if (logo.icon)
    {
        const auto tintLogo = [this]{
            const bool dark { logo.icon->colorScheme() == CZColorScheme::Dark };
            logo.icon->enableReplaceImageColor(dark);
            logo.icon->setColor(SK_ColorWHITE);
        };
        tintLogo();
        logo.icon->onColorSchemeChanged.subscribe(this, [tintLogo](CZColorScheme){ tintLogo(); });
    }

    vibrancy.layout().setWidthPercent(100.f);
    vibrancy.layout().setHeightPercent(100.f);
    vibrancy.layout().setFlexDirection(YGFlexDirectionRow);
    vibrancy.layout().setAlignItems(YGAlignCenter);


    // Application title (bold), left. Opens the default application menu.
    appTitle.setText("Desk");
    appTitle.title->setFontStyle(SkFontStyle::Bold());
    appTitle.menu = GetFirm()->appMenu->menu();

    menusArea.layout().setFlexDirection(YGFlexDirectionRow);
    menusArea.layout().setAlignItems(YGAlignCenter);
    menusArea.layout().setHeightPercent(100.f);

    outline.layout().setPositionType(YGPositionTypeAbsolute);
    outline.layout().setPosition(YGEdgeTop, 0.f);
    outline.layout().setHeightPercent(100.f);
    outline.setVisible(false);

    setColor(CZAdaptiveColor(0));
    setExclusiveEdge(CZEdgeTop);
    setLayer(Layer::Overlay);
    setAnchor(CZEdgeLeft | CZEdgeTop | CZEdgeRight);
    requestAvailableWidth();
    setMapped(true);

    // Show the active menu bar (Desk by default) on this new screen.
    GetFirm()->renderTopbar(this);

    spacer.layout().setFlex(10.f);

    // Date and time in the right corner, styled like the menu titles.
    clock.setFontStyle(SkFontStyle(
        SkFontStyle::Weight::kMedium_Weight,
        SkFontStyle::Width::kNormal_Width,
        SkFontStyle::Slant::kUpright_Slant));
    clock.setText(GetFirm()->clockText());

    // Status indicators (network, volume, battery), right before the clock. Pressing them opens the
    // control panel (the icons take no input: the whole group does).
    indicators.layout().setAlignItems(YGAlignCenter);
    updateStatus();

    // Sizes depend on the screen (very wide screens get a bigger bar).
    // Bar size menu: a gear icon tinted like the text.
    sizeItem.menu = GetFirm()->scaleMenu->menu();

    for (const char *name : { "emblem-system-symbolic", "preferences-system-symbolic", "settings-symbolic" })
        if (auto img { GetFirm()->assets.loadIcon(name, Int32(TOPBAR_ICON_SIZE * 2 * TOPBAR_MAX_SCALE)) })
        {
            sizeItem.setIcon(img);
            break;
        }

    if (sizeItem.icon)
    {
        sizeItem.icon->enableReplaceImageColor(true);
        const auto tint = [this]{ sizeItem.icon->setColor(AKTheme::Text.resolve(sizeItem.icon->colorScheme())); };
        tint();
        sizeItem.icon->onColorSchemeChanged.subscribe(this, [tint](CZColorScheme){ tint(); });
    }
    else
        sizeItem.setText("\u2699"); // No icon in the theme: ⚙
    updateUIScale();
}

void TopbarSurface::updateUIScale() noexcept
{
    const float s { GetFirm()->uiScale() };

    if (s == m_uiScale)
        return;

    m_uiScale = s;
    const float height { std::round(TOPBAR_HEIGHT * s) };
    layout().setHeight(height);
    layout().setMaxHeight(height);
    layout().setMinHeight(height);
    setExclusiveZone(Int32(height) + TOPBAR_WINDOW_GAP); // A small gap between the bar and maximized windows
    vibrancy.layout().setPadding(YGEdgeHorizontal, TOPBAR_SIDE_PADDING * s);
    clock.setFontSize(TOPBAR_FONT_SIZE * s);
    clock.layout().setMargin(YGEdgeHorizontal, TOPBAR_ITEM_PADDING * s);
    outline.setBorderRadius(CZBorderRadius::Make(Int32(std::round(4 * s))));
    logo.setUIScale(s);
    appTitle.setUIScale(s);
    sizeItem.setUIScale(s);
    indicators.layout().setMargin(YGEdgeHorizontal, TOPBAR_ITEM_PADDING * 0.5f * s);

    for (StatusIcon *icon : { &network, &volume, &battery })
        icon->setUIScale(s);

    for (AKNode *child : menusArea.children())
        if (auto *item { dynamic_cast<TopbarItem*>(child) })
            item->setUIScale(s);

    setActiveItem(nullptr);
}

void TopbarSurface::updateStatus() noexcept
{
    const Status &status { *GetFirm()->status };
    network.setIconName(status.networkIcon);
    volume.setIconName(status.volumeIcon);
    battery.setIconName(status.batteryIcon);
}

void TopbarSurface::setActiveItem(AKNode *item) noexcept
{
    if (!item)
    {
        outline.setVisible(false);
        return;
    }

    outline.layout().setPosition(YGEdgeLeft, item->worldRect().x() - TOPBAR_OUTLINE_PADDING * m_uiScale);
    outline.layout().setWidth(item->worldRect().width() + 2 * TOPBAR_OUTLINE_PADDING * m_uiScale);
    outline.setVisible(true);
}

void TopbarSurface::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    MLayerSurface::pointerButtonEvent(e);

    // The indicators: toggle the control panel on press (like macOS).
    if (e.button == BTN_LEFT && e.pressed)
    {
        for (AKNode *n { scene().nodeAt(AKApp::Get()->pointer().pos()) }; n; n = n->parent())
        {
            if (n == &indicators)
            {
                GetFirm()->toggleControlPanel(this);
                return;
            }
        }
    }

    if (e.button == BTN_LEFT && scene().nodeAt(AKApp::Get()->pointer().pos()) == &vibrancy)
        GetFirm()->setActiveTopbarItem(nullptr);
}
