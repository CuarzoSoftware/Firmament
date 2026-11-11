#include <Surfaces/ControlPanel.h>
#include <Surfaces/TopbarSurface.h>
#include <Core/Firmament.h>
#include <Core/Assets.h>
#include <Core/Status.h>
#include <Core/Controls.h>
#include <Core/CZTimer.h>
#include <Marco/MApp.h>
#include <AK/AKTheme.h>
#include <Core/Events/CZPointerButtonEvent.h>
#include <linux/input-event-codes.h>
#include <algorithm>
#include <cmath>

// Panel metrics (logical px).
static constexpr SkScalar Width { 320.f };
static constexpr SkScalar Padding { 10.f };
static constexpr SkScalar Gap { 10.f };
static constexpr Int32 Margin { 6 };           // From the bar and the screen's right edge
static constexpr Int32 CardRadius { 12 };
static constexpr SkScalar CardPadding { 10.f };
static constexpr Int32 Radius { 16 };          // The panel (rounder than menus)
static constexpr Int32 TileRadius { 9 };
static constexpr SkScalar TileHeight { 52.f };
static constexpr SkScalar BadgeSize { 30.f };
static constexpr SkScalar IconSize { 16.f };
static constexpr SkScalar ButtonSize { 30.f };
static constexpr SkScalar SliderHeight { 22.f };

// Colors.
static const CZAdaptiveColor CardColor { 0x80FFFFFF, 0x2EFFFFFF };
static const CZAdaptiveColor CardShadowColor { 0x1F000000, 0x47000000 };
static const CZAdaptiveColor TileColor { 0x00000000, 0x00000000 }; // Tiles: on their card
static const CZAdaptiveColor TileHoverColor { 0x38000000, 0x40FFFFFF };
static const CZAdaptiveColor TilePressedColor { 0x52000000, 0x5CFFFFFF };
static const CZAdaptiveColor ButtonColor { 0x12000000, 0x1AFFFFFF };
static const CZAdaptiveColor ButtonHoverColor { 0x1C000000, 0x26FFFFFF };
static const CZAdaptiveColor ButtonPressedColor { 0x29000000, 0x33FFFFFF };
static const CZAdaptiveColor BadgeOffColor { 0x1F000000, 0x2EFFFFFF };

static std::shared_ptr<RImage> LoadIcon(const char *name) noexcept
{
    return GetFirm()->assets.loadIcon(name, Int32(IconSize * 2 * TOPBAR_MAX_SCALE));
}

static void SetupIcon(AKImageFrame &icon, const char *name, SkScalar size) noexcept
{
    const SkRegion empty;
    icon.setInputRegion(&empty);
    icon.setImage(LoadIcon(name));
    icon.setSizeMode(AKImageFrame::SizeMode::Contain);
    icon.setAlignment(CZAlignCenter);
    icon.enableReplaceImageColor(true);
    icon.layout().setWidth(size);
    icon.layout().setHeight(size);
}

static void SetupText(AKText &text, SkScalar size, SkFontStyle::Weight weight, CZAdaptiveColor color) noexcept
{
    const SkRegion empty;
    text.setInputRegion(&empty);
    text.setFontSize(size);
    text.setFontStyle(SkFontStyle(weight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant));
    text.setTextColor(color);
}

/* ------------------------------------------ Panel ------------------------------------------ */

ControlPanel::ControlPanel() noexcept
{
    setBorderRadius(CZBorderRadius::Make(Radius));

    m_root.layout().setWidth(Width);
    m_root.layout().setPadding(YGEdgeAll, Padding);
    m_root.layout().setGap(YGGutterRow, Gap);

    // Header.
    m_headerCard.layout().setGap(YGGutterRow, 12.f);
    m_header.layout().setAlignItems(YGAlignCenter);
    m_header.layout().setGap(YGGutterColumn, 8.f);
    SetupIcon(m_batteryIcon, "battery-level-100-symbolic", 20.f);
    const auto tintBattery = [this]{ m_batteryIcon.setColor(AKTheme::Text.resolve(m_batteryIcon.colorScheme())); };
    tintBattery();
    m_batteryIcon.onColorSchemeChanged.subscribe(this, [tintBattery](CZColorScheme){ tintBattery(); });
    SetupText(m_batteryText, 13.f, SkFontStyle::kSemiBold_Weight, AKTheme::Label);
    m_headerSpacer.layout().setFlex(1.f);

    m_settings.onClicked = [this]{ closeAndRun([]{ GetFirm()->controls->openSettings(); }); };
    m_lock.onClicked = [this]{ closeAndRun([]{ GetFirm()->controls->lock(); }); };
    m_power.onClicked = [this]{ setPowerOptionsVisible(!m_powerRow.visible()); };

    // Power options (hidden until the power button is pressed).
    m_powerRow.layout().setJustifyContent(YGJustifySpaceAround);
    m_powerRow.setVisible(false);
    addPowerOption("weather-clear-night-symbolic", "Sleep", []{ GetFirm()->controls->suspend(); });
    addPowerOption("system-reboot-symbolic", "Restart", []{ GetFirm()->controls->restart(); });
    addPowerOption("system-log-out-symbolic", "Log Out", []{ GetFirm()->controls->logOut(); });
    addPowerOption("system-shutdown-symbolic", "Shut Down", []{ GetFirm()->controls->powerOff(); });

    // Tiles.
    m_tiles.layout().setPadding(YGEdgeAll, 4.f);
    m_tiles.layout().setGap(YGGutterRow, 2.f);

    const auto addTile = [this](const std::string &title, const char *icon) -> Tile* {
        if (m_tileNodes.size() % 2 == 0)
        {
            auto row { std::make_unique<AKContainer>(YGFlexDirectionRow, false, &m_tiles) };
            row->layout().setGap(YGGutterColumn, 2.f);
            m_tileRows.emplace_back(std::move(row));
        }

        return m_tileNodes.emplace_back(std::make_unique<Tile>(title, icon, m_tileRows.back().get())).get();
    };

    m_wifi = addTile("Wi-Fi", "network-wireless-signal-excellent-symbolic");
    m_bluetooth = addTile("Bluetooth", "bluetooth-active-symbolic");
    m_airplane = addTile("Airplane Mode", "airplane-mode-symbolic");
    m_darkMode = addTile("Dark Mode", "dark-mode-symbolic");
    m_nightLight = addTile("Night Shift", "night-light-symbolic");
    m_keyboardLight = addTile("Keyboard Light", "keyboard-brightness-symbolic");

    m_wifi->onToggled = [](bool on){ GetFirm()->controls->setWifi(on); };
    m_bluetooth->onToggled = [](bool on){ GetFirm()->controls->setBluetooth(on); };
    m_airplane->onToggled = [](bool on){ GetFirm()->controls->setAirplaneMode(on); };
    m_darkMode->onToggled = [](bool on){ GetFirm()->controls->setDarkMode(on); };
    m_nightLight->onToggled = [](bool on){ GetFirm()->controls->setNightLight(on); };
    m_keyboardLight->onToggled = [](bool on){ GetFirm()->controls->setKeyboardLight(on); };

    // An odd count: keep the last tile half wide.
    if (m_tileNodes.size() % 2 == 1)
    {
        auto *filler { new AKContainer(YGFlexDirectionRow, false, m_tileRows.back().get()) };
        filler->layout().setFlex(1.f);
    }

    // Sliders.
    for (Card *card : { &m_displayCard, &m_soundCard })
        card->layout().setGap(YGGutterRow, 8.f);

    for (AKText *label : { &m_displayLabel, &m_soundLabel })
        SetupText(*label, 12.f, SkFontStyle::kSemiBold_Weight, AKTheme::Label);

    for (AKSlider *slider : { &m_brightness, &m_volume })
    {
        slider->layout().setWidthPercent(100.f);
        slider->layout().setHeight(SliderHeight);
    }

    m_brightness.setIcon(LoadIcon("display-brightness-symbolic"));
    m_volume.setIcon(LoadIcon("audio-volume-high-symbolic"));

    m_volume.onValueChanged.subscribe(this, [](SkScalar value){ GetFirm()->status->setVolume(value); });
    m_brightness.onValueChanged.subscribe(this, [](SkScalar value){ GetFirm()->controls->setBrightness(value); });
}

void ControlPanel::openUnder(TopbarSurface *topbar, AKNode *anchor) noexcept
{
    GetFirm()->controls->refresh();
    setPowerOptionsVisible(false);
    updateStatus();

    setParent(topbar);
    refreshSize();

    // Like the bar's menus: from the item to the right (never flipped), but Margin away from the
    // bar and the screen's right edge. Moved left here to keep that margin (sliding would make it
    // touch the edge), SlideX only as a fallback.
    setAnchor(MPopup::Anchor::BottomLeft);
    setGravity(MPopup::Gravity::BottomRight);
    setConstraintAdjustment(MPopup::SlideX | MPopup::SlideY);
    setOffset(0, Margin);

    const float scale { topbar->uiScale() };
    const SkIRect r { anchor->worldRect() };
    const Int32 maxX { topbar->surfaceSize().width() - Margin - minContentSize().width() };
    const Int32 x { std::max(Margin, std::min(Int32(r.x() - TOPBAR_OUTLINE_PADDING * scale), maxX)) };
    setAnchorRect(SkIRect::MakeXYWH(x, 0, 1, Int32(std::round(TOPBAR_HEIGHT * scale))));
    setMapped(true);
}

void ControlPanel::refreshSize() noexcept
{
    const SkISize s { minContentSize() };
    layout().setWidth(s.width());
    layout().setHeight(s.height());
}

void ControlPanel::updateStatus() noexcept
{
    const Status &status { *GetFirm()->status };

    // Battery: its icon and level.
    const bool battery { status.batteryPercent >= 0 };
    m_batteryIcon.setVisible(battery);
    m_batteryText.setVisible(battery);

    if (battery)
    {
        if (!status.batteryIcon.empty())
            m_batteryIcon.setImage(LoadIcon(status.batteryIcon.c_str()));

        m_batteryText.setText(std::to_string(status.batteryPercent) + " %");
    }

    // Volume (not while being dragged, see AKSlider::setValue()).
    m_volume.setVisible(status.volume >= 0.f);
    m_soundLabel.setVisible(status.volume >= 0.f);

    if (status.volume >= 0.f)
    {
        m_volume.setValue(status.muted ? 0.f : status.volume);

        if (!status.volumeIcon.empty())
            m_volume.setIcon(LoadIcon(status.volumeIcon.c_str()));
    }

    // Controls.
    const Controls &c { *GetFirm()->controls };
    m_wifi->setAvailable(c.wifiAvailable);
    m_wifi->setToggled(c.wifiEnabled);
    m_bluetooth->setAvailable(c.bluetoothAvailable);
    m_bluetooth->setToggled(c.bluetoothEnabled);
    m_airplane->setAvailable(c.airplaneAvailable);
    m_airplane->setToggled(c.airplaneMode);
    m_keyboardLight->setAvailable(c.keyboardLightAvailable);
    m_keyboardLight->setToggled(c.keyboardLightOn);
    m_darkMode->setToggled(c.darkMode());
    m_nightLight->setAvailable(c.nightLightAvailable);
    m_nightLight->setToggled(c.nightLightOn);

    const bool backlight { c.brightness >= 0.f };
    m_brightness.setVisible(backlight);
    m_displayLabel.setVisible(backlight);

    if (backlight)
        m_brightness.setValue(c.brightness);
}

void ControlPanel::closeAndRun(std::function<void()> action) noexcept
{
    GetFirm()->closeControlPanel();

    // Once the panel is gone (e.g. not captured by the lock screen's first frame).
    CZTimer::OneShot(50, [action](CZTimer*){ action(); });
}

void ControlPanel::addPowerOption(const char *icon, const char *label, std::function<void()> action) noexcept
{
    auto option { std::make_unique<AKContainer>(YGFlexDirectionColumn, false, &m_powerRow) };
    option->layout().setAlignItems(YGAlignCenter);
    option->layout().setGap(YGGutterRow, 4.f);

    auto *button { new RoundButton(icon, option.get()) };
    button->layout().setWidth(36.f);
    button->layout().setHeight(36.f);
    button->setBorderRadius(CZBorderRadius::Make(18));
    button->onClicked = [this, action]{ closeAndRun(action); };

    auto *text { new AKText(label, option.get()) };
    SetupText(*text, 11.f, SkFontStyle::kNormal_Weight, AKTheme::Label);

    m_powerOptions.emplace_back(std::move(option));
}

void ControlPanel::setPowerOptionsVisible(bool visible) noexcept
{
    if (m_powerRow.visible() == visible)
        return;

    m_powerRow.setVisible(visible);

    // Grow/shrink the popup to its content.
    if (mapped())
        refreshSize();
}

/* ------------------------------------------ Tile ------------------------------------------ */

ControlPanel::Tile::Tile(const std::string &title, const char *icon, AKNode *parent) noexcept :
    AKRoundSolidColor(0, CZBorderRadius::Make(TileRadius), parent),
    m_badge(0, CZBorderRadius::Make(Int32(BadgeSize / 2)), this)
{
    layout().setFlex(1.f);
    layout().setFlexBasis(0.f);
    layout().setHeight(TileHeight);
    layout().setFlexDirection(YGFlexDirectionRow);
    layout().setAlignItems(YGAlignCenter);
    layout().setPadding(YGEdgeHorizontal, 10.f);
    layout().setGap(YGGutterColumn, 9.f);

    const SkRegion empty;
    m_badge.setInputRegion(&empty);
    m_badge.layout().setWidth(BadgeSize);
    m_badge.layout().setHeight(BadgeSize);
    m_badge.layout().setJustifyContent(YGJustifyCenter);
    m_badge.layout().setAlignItems(YGAlignCenter);
    SetupIcon(m_icon, icon, IconSize);

    m_texts.setInputRegion(&empty);
    m_texts.layout().setFlex(1.f);
    SetupText(m_title, 12.f, SkFontStyle::kSemiBold_Weight, AKTheme::Label);
    SetupText(m_state, 11.f, SkFontStyle::kNormal_Weight, AKTheme::SecondaryLabel);
    m_title.setText(title);

    updateColors();
}

void ControlPanel::Tile::setToggled(bool toggled) noexcept
{
    if (m_toggled == toggled && !m_state.text().empty())
        return;

    m_toggled = toggled;
    updateColors();
}

void ControlPanel::Tile::setAvailable(bool available) noexcept
{
    if (m_available == available)
        return;

    m_available = available;
    setOpacity(available ? 1.f : 0.45f);

    if (!available)
        m_state.setText("Unavailable");
    else
        updateColors();
}

void ControlPanel::Tile::updateColors() noexcept
{
    const CZColorScheme scheme { colorScheme() };
    const auto &bg { m_pressed ? TilePressedColor : m_hover ? TileHoverColor : TileColor };
    setBackgroundColor(bg.resolve(scheme));
    m_badge.setBackgroundColor((m_toggled ? AKTheme::SystemBlue : BadgeOffColor).resolve(scheme));
    m_icon.setColor((m_toggled ? AKTheme::White : AKTheme::Label).resolve(scheme));
    m_state.setText(!m_available ? "Unavailable" : m_toggled ? "On" : "Off");
}

void ControlPanel::Tile::pointerEnterEvent(const CZPointerEnterEvent &e)
{
    AKRoundSolidColor::pointerEnterEvent(e);
    m_hover = true;
    updateColors();
}

void ControlPanel::Tile::pointerLeaveEvent(const CZPointerLeaveEvent &e)
{
    AKRoundSolidColor::pointerLeaveEvent(e);
    m_hover = m_pressed = false;
    updateColors();
}

void ControlPanel::Tile::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    AKRoundSolidColor::pointerButtonEvent(e);

    if (e.button != BTN_LEFT || !m_available)
        return;

    if (e.pressed)
        m_pressed = true;
    else if (m_pressed)
    {
        // Optimistic: the real state follows (Controls::onChanged).
        m_pressed = false;
        m_toggled = !m_toggled;

        if (onToggled)
            onToggled(m_toggled);
    }

    updateColors();
}

void ControlPanel::Tile::colorSchemeEvent(const CZColorSchemeEvent &e)
{
    AKRoundSolidColor::colorSchemeEvent(e);
    updateColors();
}

/* ------------------------------------------ Card ------------------------------------------ */

ControlPanel::Card::Card(YGFlexDirection direction, AKNode *parent) noexcept :
    AKRoundSolidColor(0, CZBorderRadius::Make(CardRadius), parent),
    m_shadow(6.f, SkIPoint::Make(0, 1), 0, false, this)
{
    layout().setFlexDirection(direction);
    layout().setPadding(YGEdgeAll, CardPadding);
    m_shadow.setBorderRadius(CZBorderRadius::Make(CardRadius));
    updateColors();
}

void ControlPanel::Card::updateColors() noexcept
{
    const CZColorScheme scheme { colorScheme() };
    setBackgroundColor(CardColor.resolve(scheme));
    m_shadow.setColor(CardShadowColor.resolve(scheme));
}

void ControlPanel::Card::colorSchemeEvent(const CZColorSchemeEvent &e)
{
    AKRoundSolidColor::colorSchemeEvent(e);
    updateColors();
}

/* --------------------------------------- RoundButton --------------------------------------- */

ControlPanel::RoundButton::RoundButton(const char *icon, AKNode *parent) noexcept :
    AKRoundSolidColor(0, CZBorderRadius::Make(Int32(ButtonSize / 2)), parent)
{
    layout().setWidth(ButtonSize);
    layout().setHeight(ButtonSize);
    layout().setJustifyContent(YGJustifyCenter);
    layout().setAlignItems(YGAlignCenter);
    SetupIcon(m_icon, icon, IconSize);
    updateColors();
}

void ControlPanel::RoundButton::updateColors() noexcept
{
    const CZColorScheme scheme { colorScheme() };
    setBackgroundColor((m_pressed ? ButtonPressedColor : m_hover ? ButtonHoverColor : ButtonColor).resolve(scheme));
    m_icon.setColor(AKTheme::Label.resolve(scheme));
}

void ControlPanel::RoundButton::pointerEnterEvent(const CZPointerEnterEvent &e)
{
    AKRoundSolidColor::pointerEnterEvent(e);
    m_hover = true;
    updateColors();
}

void ControlPanel::RoundButton::pointerLeaveEvent(const CZPointerLeaveEvent &e)
{
    AKRoundSolidColor::pointerLeaveEvent(e);
    m_hover = m_pressed = false;
    updateColors();
}

void ControlPanel::RoundButton::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    AKRoundSolidColor::pointerButtonEvent(e);

    if (e.button != BTN_LEFT)
        return;

    if (e.pressed)
        m_pressed = true;
    else if (m_pressed)
    {
        m_pressed = false;

        if (onClicked)
            onClicked();
    }

    updateColors();
}

void ControlPanel::RoundButton::colorSchemeEvent(const CZColorSchemeEvent &e)
{
    AKRoundSolidColor::colorSchemeEvent(e);
    updateColors();
}
