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
#include <AK/AKApp.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RImage.h>
#include <CZ/skia/core/SkBitmap.h>
#include <CZ/skia/core/SkCanvas.h>
#include <CZ/skia/core/SkRRect.h>
#include <drm_fourcc.h>
#include <algorithm>
#include <cmath>

// Panel metrics (logical px). Width is the minimum: wider if the tiles' labels need it.
static constexpr SkScalar Width { 320.f };
static constexpr SkScalar Padding { 10.f };
static constexpr SkScalar Gap { 10.f };
static constexpr Int32 Margin { 6 };           // From the bar and the screen's right edge
static constexpr Int32 CardRadius { 12 };
static constexpr SkScalar CardPadding { 10.f };
static constexpr Int32 Radius { 16 };          // The panel (rounder than menus)
static constexpr Int32 TileRadius { 9 };
static constexpr SkScalar TileHeight { 52.f };
static constexpr SkScalar TilePadding { 10.f };
static constexpr SkScalar TileGap { 9.f };      // Between the badge and the texts
static constexpr SkScalar BadgeSize { 30.f };
static constexpr SkScalar IconSize { 16.f };
static constexpr SkScalar ButtonSize { 30.f };
static constexpr SkScalar SliderHeight { 22.f };
static constexpr SkScalar CaretSize { 18.f };
static constexpr SkScalar RowHeight { 30.f };       // Wi-Fi networks
static constexpr size_t MaxNetworks { 40 };
static constexpr SkScalar MaxListHeight { RowHeight * 6.5f }; // A row cut in half: it scrolls
static constexpr UInt32 ScanIntervalMs { 15000 };   // While the list is shown
static constexpr UInt32 MessageMs { 5000 };
static constexpr size_t MaxSsidChars { 26 };

// Colors.
static const CZAdaptiveColor CardColor { 0x80FFFFFF, 0x2EFFFFFF };
static const CZAdaptiveColor CardShadowColor { 0x1F000000, 0x47000000 };
static const CZAdaptiveColor TileColor { 0x00000000, 0x00000000 }; // Tiles: on their card
static const CZAdaptiveColor TileHoverColor { 0x38000000, 0x40FFFFFF };
static const CZAdaptiveColor TilePressedColor { 0x52000000, 0x5CFFFFFF };
static const CZAdaptiveColor BadgeOffColor { 0x1F000000, 0x2EFFFFFF };

// Long names cut (with an ellipsis): the panel keeps its width.
static std::string ShortName(const std::string &name, size_t max = MaxSsidChars) noexcept
{
    size_t chars { 0 }, i { 0 };

    for (; i < name.size(); i++)
        if ((name[i] & 0xC0) != 0x80 && chars++ == max)
            return name.substr(0, i) + "…";

    return name;
}

static constexpr SkScalar ArtworkSize { 46.f };
static constexpr size_t MaxNowPlayingChars { 24 };

// A rounded square (white, antialiased, on transparent) of @p size pixels: the artwork's mask, drawn
// on the CPU at the size it's shown at (see AKAvatar).
static std::shared_ptr<RImage> RoundedMask(Int32 size, SkScalar radius) noexcept
{
    auto ream { RCore::Get() };

    if (!ream || !ream->mainDevice() || size <= 0)
        return {};

    const auto &formats { ream->mainDevice()->textureFormats().formats() };
    const auto format { formats.find(DRM_FORMAT_ABGR8888) };

    if (format == formats.end())
        return {};

    SkBitmap bitmap;

    if (!bitmap.tryAllocPixels(SkImageInfo::Make(size, size, kRGBA_8888_SkColorType, kPremul_SkAlphaType)))
        return {};

    {
        SkCanvas c { bitmap };
        c.clear(SK_ColorTRANSPARENT);
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(SK_ColorWHITE);
        c.drawRRect(SkRRect::MakeRectXY(SkRect::MakeWH(size, size), radius, radius), paint);
    }

    RPixelBufferInfo info {};
    info.pixels = static_cast<UInt8*>(bitmap.getPixels());
    info.stride = UInt32(bitmap.rowBytes());
    info.format = format->format();
    info.size = SkISize::Make(size, size);
    return RImage::MakeFromPixels(info, *format);
}

// The picture at @p path, or nullptr
static std::shared_ptr<RImage> LoadPicture(const std::string &path) noexcept
{
    auto ream { RCore::Get() };

    if (path.empty() || !ream || !ream->mainDevice())
        return {};

    const auto &formats { ream->mainDevice()->textureFormats().formats() };

    if (auto fmt { formats.find(DRM_FORMAT_ARGB8888) }; fmt != formats.end())
        return RImage::LoadFile(path, *fmt);

    return {};
}

static std::shared_ptr<RImage> LoadIcon(const char *name) noexcept
{
    return GetFirm()->assets.loadIcon(name, Int32(IconSize * 2 * TOPBAR_MAX_SCALE));
}

static void SetupIcon(AKImageFrame &icon, const char *name, SkScalar size) noexcept
{
    const SkRegion empty;
    icon.setInputRegion(&empty);
    GetFirm()->assets.show(icon, LoadIcon(name));
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
    setBlurMaterial(MBlurMaterial::Popover);
    setBorderRadius(CZBorderRadius::Make(Radius));

    m_root.layout().setMinWidth(Width); // Wider if a tile's label needs it (see below)
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
    m_performance = addTile("High Performance", "power-profile-performance-symbolic");
    m_darkMode = addTile("Dark Mode", "dark-mode-symbolic");
    m_nightLight = addTile("Night Shift", "night-light-symbolic");
    m_keyboardLight = addTile("Keyboard Light", "keyboard-brightness-symbolic");

    m_wifi->onToggled = [](bool on){ GetFirm()->controls->setWifi(on); };
    m_wifi->setExpandable(true);
    m_wifi->onExpand = [this]{ setWiFiExpanded(!m_wifiExpanded); };
    m_bluetooth->onToggled = [](bool on){ GetFirm()->controls->setBluetooth(on); };
    m_performance->onToggled = [](bool on){ GetFirm()->controls->setPerformance(on); };
    m_darkMode->onToggled = [](bool on){ GetFirm()->controls->setDarkMode(on); };
    m_nightLight->onToggled = [](bool on){ GetFirm()->controls->setNightLight(on); };
    m_keyboardLight->onToggled = [](bool on){ GetFirm()->controls->setKeyboardLight(on); };

    // Every tile as wide as the widest one needs (equal columns): the panel grows instead of
    // labels going out of their tile.
    SkScalar tileWidth { 0.f };

    for (const auto &tile : m_tileNodes)
        tileWidth = std::max(tileWidth, tile->contentWidth());

    for (auto &tile : m_tileNodes)
        tile->layout().setMinWidth(std::ceil(tileWidth));

    // An odd count: keep the last tile half wide.
    if (m_tileNodes.size() % 2 == 1)
    {
        auto *filler { new AKContainer(YGFlexDirectionRow, false, m_tileRows.back().get()) };
        filler->layout().setFlex(1.f);
        filler->layout().setFlexBasis(0.f);
        filler->layout().setMinWidth(std::ceil(tileWidth));
    }

    // Wi-Fi networks (hidden until the Wi-Fi tile's caret is pressed).
    const SkRegion noInput;
    m_wifiCard.insertAfter(m_tileRows.front().get()); // Right under the Wi-Fi tile
    m_wifiCard.setVisible(false);
    m_wifiCard.layout().setPadding(YGEdgeHorizontal, 6.f);
    m_wifiCard.layout().setPadding(YGEdgeBottom, 6.f);
    m_wifiCard.layout().setGap(YGGutterRow, 2.f);
    m_networkScroll.layout().setWidthPercent(100.f);
    m_networkScroll.verticalBar().setRailVisible(false);
    m_networkScroll.horizontalBar().setRailVisible(false);
    m_networkList.layout().setWidthPercent(100.f);
    m_networkList.layout().setPadding(YGEdgeRight, 12.f); // The scroll bar never over the rows
    m_wifiHeader.setInputRegion(&noInput);
    m_wifiHeader.layout().setAlignItems(YGAlignCenter);
    m_wifiHeader.layout().setPadding(YGEdgeHorizontal, 6.f);
    m_wifiHeader.layout().setPadding(YGEdgeVertical, 4.f);
    m_wifiHeaderSpacer.layout().setFlex(1.f);
    m_wifiSpinner.setInputRegion(&noInput);
    m_wifiSpinner.setVisible(false);
    SetupText(m_wifiTitle, 12.f, SkFontStyle::kSemiBold_Weight, AKTheme::Label);
    SetupText(m_wifiMessage, 11.f, SkFontStyle::kNormal_Weight, AKTheme::SecondaryLabel);
    m_wifiMessage.layout().setPadding(YGEdgeHorizontal, 6.f);
    m_wifiMessage.layout().setPadding(YGEdgeBottom, 4.f);

    m_passwordBox.setVisible(false);
    m_passwordBox.layout().setPadding(YGEdgeAll, 6.f);
    m_passwordBox.layout().setGap(YGGutterRow, 6.f);
    SetupText(m_passwordLabel, 11.f, SkFontStyle::kNormal_Weight, AKTheme::SecondaryLabel);
    m_passwordRow.layout().setAlignItems(YGAlignCenter);
    m_passwordRow.layout().setGap(YGGutterColumn, 6.f);
    m_password.layout().setFlex(1.f);
    m_password.layout().setWidth(10.f); // Grows (flex), its placeholder doesn't widen the panel
    m_password.setPasswordMode(true);
    m_password.setShowsVisibilityToggle(true);
    m_joinButton.setEnabled(false);
    m_password.onTextChanged.subscribe(this, [this](const std::string &text){ m_joinButton.setEnabled(text.size() >= 8); });
    m_password.onSubmit.subscribe(this, [this](const std::string &){ joinWithPassword(); });
    m_password.onCancel.subscribe(this, [this]{ hidePassword(); });
    m_joinButton.onClick.subscribe(this, [this](const CZPointerButtonEvent &){ joinWithPassword(); });

    m_wifiSettings.setAction("Wi-Fi Settings…");
    m_wifiSettings.onClicked = [this]{ closeAndRun([]{ GetFirm()->controls->openWiFiSettings(); }); };

    m_scanTimer = std::make_unique<CZTimer>([this](CZTimer *t){
        if (m_wifiBackend)
            m_wifiBackend->scan();
        t->start(ScanIntervalMs);
    });

    m_messageTimer = std::make_unique<CZTimer>([this](CZTimer*){
        m_message.clear();
        updateWiFiList();
    });

    // Closed: collapsed (and no more scans).
    onMappedChanged.subscribe(this, [this]{
        if (!mapped())
            setWiFiExpanded(false);
    });

    // Sliders.
    for (Card *card : { &m_displayCard, &m_soundCard, &m_micCard })
        card->layout().setGap(YGGutterRow, 8.f);

    for (AKText *label : { &m_displayLabel, &m_soundLabel, &m_micLabel })
        SetupText(*label, 12.f, SkFontStyle::kSemiBold_Weight, AKTheme::Label);

    m_brightness.layout().setWidthPercent(100.f);
    m_brightness.layout().setHeight(SliderHeight);

    // Volume sliders: their device button at the end, the list (hidden) below
    for (AKContainer *row : { &m_volumeRow, &m_micRow })
    {
        row->layout().setAlignItems(YGAlignCenter);
        row->layout().setGap(YGGutterColumn, 8.f);
    }

    for (AKSlider *slider : { &m_volume, &m_micVolume })
    {
        slider->layout().setFlex(1.f);
        slider->layout().setHeight(SliderHeight);
    }

    for (RoundButton *button : { &m_outputButton, &m_inputButton })
    {
        button->layout().setWidth(SliderHeight + 2.f);
        button->layout().setHeight(SliderHeight + 2.f);
        button->setBorderRadius(CZBorderRadius::Make(Int32(SliderHeight / 2 + 1)));
    }

    m_outputButton.onClicked = [this]{ openAudioMenu(false); };
    m_inputButton.onClicked = [this]{ openAudioMenu(true); };
    m_micVolume.setIconProvider(GetFirm()->assets.provider(LoadIcon("microphone-sensitivity-high-symbolic")));
    m_micVolume.onValueChanged.subscribe(this, [this](SkScalar value){
        if (m_audio)
            m_audio->setVolume(true, value);
    });

    // Now Playing (hidden without a player)
    m_nowCard.setVisible(false);
    m_nowCard.layout().setAlignItems(YGAlignCenter);
    m_nowCard.layout().setGap(YGGutterColumn, 6.f);
    m_nowInfo.layout().setFlex(1.f);
    m_nowInfo.layout().setAlignItems(YGAlignCenter);
    m_nowInfo.layout().setGap(YGGutterColumn, 10.f);
    m_nowInfo.layout().setMinWidth(0.f);
    m_artFrame.layout().setWidth(ArtworkSize);
    m_artFrame.layout().setHeight(ArtworkSize);
    m_artFrame.layout().setJustifyContent(YGJustifyCenter);
    m_artFrame.layout().setAlignItems(YGAlignCenter);
    SetupIcon(m_artPlaceholder, "audio-x-generic-symbolic", 22.f);
    {
        const SkRegion empty;
        m_artFrame.setInputRegion(&empty);
        m_artwork.setInputRegion(&empty);
        m_nowTexts.setInputRegion(&empty);
    }
    m_artwork.layout().setPositionType(YGPositionTypeAbsolute);
    m_artwork.layout().setPosition(YGEdgeLeft, 0.f);
    m_artwork.layout().setPosition(YGEdgeTop, 0.f);
    m_artwork.layout().setWidthPercent(100.f);
    m_artwork.layout().setHeightPercent(100.f);
    m_artwork.setSrcRectMode(AKImage::SrcRectMode::Custom);
    m_artwork.setVisible(false);
    m_nowTexts.layout().setFlex(1.f);
    m_nowTexts.layout().setGap(YGGutterRow, 2.f);
    SetupText(m_nowTitle, 13.f, SkFontStyle::kSemiBold_Weight, AKTheme::Label);
    SetupText(m_nowArtist, 11.f, SkFontStyle::kNormal_Weight, AKTheme::SecondaryLabel);

    for (RoundButton *button : { &m_previous, &m_playPause, &m_next })
    {
        button->layout().setWidth(28.f);
        button->layout().setHeight(28.f);
        button->setBorderRadius(CZBorderRadius::Make(14));
    }

    m_nowPlaying = std::make_unique<NowPlaying>();
    m_nowPlaying->onChanged.subscribe(this, [this]{ updateNowPlaying(); });
    m_previous.onClicked = [this]{ m_nowPlaying->previous(); };
    m_playPause.onClicked = [this]{ m_nowPlaying->playPause(); };
    m_next.onClicked = [this]{ m_nowPlaying->next(); };
    m_nowInfo.onClicked = [this]{
        if (m_nowPlaying->state().canRaise)
            closeAndRun([this]{ m_nowPlaying->raise(); });
    };
    updateNowPlaying();

    m_audio = std::make_unique<Audio>();
    m_audio->onChanged.subscribe(this, [this]{ updateAudio(); });
    updateAudio();

    onMappedChanged.subscribe(this, [this]{
        if (!mapped() && m_audioMenu)
            m_audioMenu->setMapped(false);
    });

    m_brightness.setIconProvider(GetFirm()->assets.provider(LoadIcon("display-brightness-symbolic")));
    m_volume.setIconProvider(GetFirm()->assets.provider(LoadIcon("audio-volume-high-symbolic")));

    m_volume.onValueChanged.subscribe(this, [](SkScalar value){ GetFirm()->status->setVolume(value); });
    m_brightness.onValueChanged.subscribe(this, [](SkScalar value){ GetFirm()->controls->setBrightness(value); });
}

void ControlPanel::openUnder(TopbarSurface *topbar, AKNode *anchor, const CZInputEvent *grab) noexcept
{
    GetFirm()->controls->refresh();
    setPowerOptionsVisible(false);
    setWiFiExpanded(false);
    setGrab(grab);
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
            GetFirm()->assets.show(m_batteryIcon, LoadIcon(status.batteryIcon.c_str()));

        m_batteryText.setText(std::to_string(status.batteryPercent) + " %");
    }

    // Volume (not while being dragged, see AKSlider::setValue()).
    m_soundCard.setVisible(status.volume >= 0.f);

    if (status.volume >= 0.f)
    {
        m_volume.setValue(status.muted ? 0.f : status.volume);

        if (!status.volumeIcon.empty())
            m_volume.setIconProvider(GetFirm()->assets.provider(LoadIcon(status.volumeIcon.c_str())));
    }

    // Controls.
    const Controls &c { *GetFirm()->controls };
    m_wifi->setAvailable(c.wifiAvailable);
    m_wifi->setToggled(c.wifiEnabled);

    if (!c.wifiAvailable && m_wifiExpanded)
        setWiFiExpanded(false);
    m_bluetooth->setAvailable(c.bluetoothAvailable);
    m_bluetooth->setToggled(c.bluetoothEnabled);
    m_performance->setAvailable(c.performanceAvailable);
    m_performance->setToggled(c.powerProfile == "performance");
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

/* ------------------------------------------- Audio ------------------------------------------- */

void ControlPanel::updateAudio() noexcept
{
    const AudioState &s { m_audio->state() };

    // The microphone: hidden without one
    const bool mic { s.input.valid && s.input.volume >= 0.f };
    m_micCard.setVisible(mic);

    if (mic)
    {
        const float v { s.input.muted ? 0.f : s.input.volume };
        m_micVolume.setValue(v);
        m_micVolume.setIconProvider(GetFirm()->assets.provider(LoadIcon(
            v <= 0.f ? "microphone-sensitivity-muted-symbolic" : v < 0.34f ? "microphone-sensitivity-low-symbolic" :
            v < 0.67f ? "microphone-sensitivity-medium-symbolic" : "microphone-sensitivity-high-symbolic")));
    }

    m_outputButton.setVisible(s.available);
    m_inputButton.setVisible(s.available);

    if (mapped())
        refreshSize();
}

void ControlPanel::openAudioMenu(bool input) noexcept
{
    if (!m_audioMenu)
        m_audioMenu = std::make_unique<MMenu>();

    MMenu &menu { *m_audioMenu };
    menu.setMapped(false);
    menu.clear();

    menu.addAction("Advanced Settings…", [this]{ closeAndRun([]{ GetFirm()->controls->openSoundSettings(); }); });
    menu.addDivider();

    const AudioState &s { m_audio->state() };
    const AudioSelection &sel { input ? s.input : s.output };
    const auto &devices { input ? s.inputs : s.outputs };

    if (devices.empty())
        menu.addAction(input ? "No Inputs" : "No Outputs", {}, false);

    for (const AudioDevice &d : devices)
    {
        const bool current { sel.device == d.name };
        menu.addAction(d.displayName, [this, input, name = d.name]{ m_audio->select(input, name); })
            .setChecked(current && sel.channels.empty());

        // Multichannel: pairs of outputs (1+2, 3+4...), single inputs, indented under it
        const UInt32 n { UInt32(d.channels.size()) };

        if ((!input && n > 2) || (input && n > 1))
        {
            for (UInt32 c = 0; c < n; c += input ? 1 : 2)
            {
                std::vector<UInt32> channels { c };

                if (!input && c + 1 < n)
                    channels.push_back(c + 1);

                MMenuAction &action { menu.addAction(AudioState::ChannelsName(input, channels),
                    [this, input, name = d.name, channels]{ m_audio->select(input, name, channels); }) };
                action.setChecked(current && sel.channels == channels);
                action.text.layout().setMargin(YGEdgeLeft, 14.f);
            }
        }
    }

    // A child of the panel, grabbing the keyboard from it (the click that opened it, if any)
    const auto &click { AKApp::Get()->pointer().history().button };
    menu.popup(input ? m_inputButton : m_outputButton, click.serial ? &click : nullptr);
}

/* ---------------------------------------- Now Playing ---------------------------------------- */

void ControlPanel::updateNowPlaying() noexcept
{
    const NowPlaying::State &s { m_nowPlaying->state() };
    const bool shown { s.available && (!s.title.empty() || !s.identity.empty()) };
    m_nowCard.setVisible(shown);

    if (shown)
    {
        m_nowTitle.setText(ShortName(s.title.empty() ? s.identity : s.title, MaxNowPlayingChars));
        m_nowArtist.setText(ShortName(s.artist.empty() ? (s.title.empty() ? "" : s.identity) : s.artist, MaxNowPlayingChars));
        m_nowArtist.setVisible(!m_nowArtist.text().empty());
        m_playPause.setIcon(s.playing ? "media-playback-pause-symbolic" : "media-playback-start-symbolic");
        m_previous.setOpacity(s.canPrevious ? 1.f : 0.35f);
        m_next.setOpacity(s.canNext ? 1.f : 0.35f);
        m_playPause.setOpacity(s.canPlayPause ? 1.f : 0.35f);

        // The artwork (its centered square), or the placeholder
        if (s.artwork != m_artworkPath)
        {
            m_artworkPath = s.artwork;
            auto picture { LoadPicture(m_artworkPath) };
            m_artwork.setImage(picture);
            m_artwork.setVisible(picture != nullptr);
            m_artPlaceholder.setVisible(picture == nullptr);

            if (picture)
            {
                const SkISize size { picture->size() };
                const Int32 side { std::min(size.width(), size.height()) };
                m_artwork.setCustomSrcRectScale(1.f);
                m_artwork.setCustomSrcRect(SkRect::MakeXYWH((size.width() - side) / 2.f, (size.height() - side) / 2.f, side, side));
                const Int32 px { Int32(ArtworkSize) * std::max(1, scale()) };
                m_artwork.setMask(RoundedMask(px, 8.f * std::max(1, scale())));
            }
        }

        m_artFrame.setBackgroundColor(AKTheme::ControlFill.resolve(colorScheme()));
    }

    if (mapped())
        refreshSize();
}

void ControlPanel::ClickArea::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    AKContainer::pointerButtonEvent(e);

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
}

/* ---------------------------------------- Wi-Fi list ---------------------------------------- */

void ControlPanel::setWiFiExpanded(bool expanded) noexcept
{
    if (m_wifiExpanded == expanded)
        return;

    m_wifiExpanded = expanded;
    m_wifi->setExpanded(expanded);
    m_wifiCard.setVisible(expanded);

    if (expanded)
    {
        if (!m_wifiBackend)
        {
            m_wifiBackend = std::make_unique<WiFi>();
            m_wifiBackend->onChanged.subscribe(this, [this]{ updateWiFiList(); });
            m_wifiBackend->onJoinFailed.subscribe(this, [this](const std::string &ssid, bool wrongPassword){
                if (wrongPassword)
                {
                    for (const auto &n : m_wifiBackend->state().networks)
                    {
                        if (n.ssid != ssid)
                            continue;

                        // A saved password that no longer works: asked again (saved as a new profile)
                        if (n.known())
                            m_wifiBackend->forget(n);

                        if (m_wifiExpanded)
                            askPassword(n, true);
                        return;
                    }
                }

                showWiFiMessage("Couldn't join \"" + ShortName(ssid) + "\".");
            });
        }

        m_wifiBackend->scan();
        m_scanTimer->start(ScanIntervalMs);
        m_networkScroll.setOffset(0.f, 0.f);
        updateWiFiList();
    }
    else
    {
        m_scanTimer->stop();
        hidePassword();
    }

    if (mapped())
        refreshSize();
}

void ControlPanel::updateWiFiList() noexcept
{
    if (!m_wifiBackend || !m_wifiExpanded)
        return;

    const WiFiState &s { m_wifiBackend->state() };
    const bool on { s.available && s.enabled };

    // The one it's connected to first, then the known ones, then by strength.
    std::vector<const WiFiNetwork*> networks;

    if (on)
        for (const auto &n : s.networks)
            if (!n.ssid.empty())
                networks.push_back(&n);

    std::stable_sort(networks.begin(), networks.end(), [&s](const WiFiNetwork *a, const WiFiNetwork *b){
        const bool aActive { a->ssid == s.activeSsid }, bActive { b->ssid == s.activeSsid };
        if (aActive != bActive) return aActive;
        if (a->known() != b->known()) return a->known();
        return a->strength > b->strength;
    });

    if (networks.size() > MaxNetworks)
        networks.resize(MaxNetworks);

    while (m_networkRows.size() > networks.size())
        m_networkRows.pop_back();

    while (m_networkRows.size() < networks.size())
    {
        auto &row { m_networkRows.emplace_back(std::make_unique<NetworkRow>(&m_networkList)) };
        NetworkRow *raw { row.get() };
        row->onClicked = [this, raw]{ joinNetwork(raw->network); };
    }

    const bool connecting { s.link == WiFiState::Link::Connecting };

    for (size_t i = 0; i < networks.size(); i++)
    {
        const bool active { networks[i]->ssid == s.activeSsid };
        m_networkRows[i]->set(*networks[i], active && !connecting, active && connecting);
    }

    // Its rows' height, up to MaxListHeight (then it scrolls)
    m_networkScroll.setVisible(!networks.empty());
    m_networkScroll.layout().setHeight(std::min(MaxListHeight, RowHeight * SkScalar(networks.size())));

    // Off, searching, or a message (e.g. why it couldn't join)
    std::string message { m_message };

    if (message.empty())
    {
        if (!s.loaded)
            message = "";
        else if (!s.available)
            message = "No Wi-Fi device.";
        else if (!s.enabled)
            message = "Wi-Fi is off.";
        else if (networks.empty())
            message = "Looking for networks…";
    }

    m_wifiMessage.setText(message);
    m_wifiMessage.setVisible(!message.empty());

    const bool busy { on && (networks.empty() || connecting) };
    m_wifiSpinner.setVisible(busy);

    if (busy) m_wifiSpinner.start();
    else      m_wifiSpinner.stop();

    if (mapped())
        refreshSize();
}

void ControlPanel::joinNetwork(const WiFiNetwork &network) noexcept
{
    if (!m_wifiBackend || network.ssid == m_wifiBackend->state().activeSsid)
        return;

    // Not supported here (nor in System Settings yet, which says so).
    if (network.enterprise || network.wep)
    {
        showWiFiMessage(network.securityName() + " networks aren't supported yet.");
        return;
    }

    if (network.secured && !network.known())
    {
        askPassword(network, false);
        return;
    }

    hidePassword();
    m_message.clear();
    m_wifiBackend->join(network);
}

void ControlPanel::askPassword(const WiFiNetwork &network, bool wrong) noexcept
{
    m_passwordNetwork = network;
    m_passwordLabel.setText(wrong ?
        "Incorrect password for \"" + ShortName(network.ssid) + "\". Try again." :
        "Enter the password for \"" + ShortName(network.ssid) + "\".");
    m_password.setText("");
    m_joinButton.setEnabled(false);
    m_passwordBox.setVisible(true);
    m_password.focus();

    if (mapped())
        refreshSize();
}

void ControlPanel::hidePassword() noexcept
{
    if (!m_passwordBox.visible())
        return;

    m_password.setText("");
    m_password.editor.setKeyboardFocus(false);
    m_passwordBox.setVisible(false);
    m_passwordNetwork = {};

    if (mapped())
        refreshSize();
}

void ControlPanel::joinWithPassword() noexcept
{
    // WPA passwords have 8 to 63 characters
    if (!m_wifiBackend || m_password.text().size() < 8)
        return;

    const WiFiNetwork network { m_passwordNetwork };
    const std::string password { m_password.text() };
    hidePassword();
    m_message.clear();
    m_wifiBackend->join(network, password);
}

void ControlPanel::showWiFiMessage(const std::string &message) noexcept
{
    m_message = message;
    m_messageTimer->start(MessageMs);
    updateWiFiList();
}

/* ---------------------------------------- NetworkRow ---------------------------------------- */

ControlPanel::NetworkRow::NetworkRow(AKNode *parent) noexcept :
    AKRoundSolidColor(0, CZBorderRadius::Make(7), parent)
{
    layout().setFlexDirection(YGFlexDirectionRow);
    layout().setAlignItems(YGAlignCenter);
    layout().setHeight(RowHeight);
    layout().setPadding(YGEdgeHorizontal, 6.f);
    layout().setGap(YGGutterColumn, 8.f);

    const SkRegion empty;

    for (AKNode *n : std::initializer_list<AKNode*>{ &m_signal, &m_name, &m_lock, &m_spinner, &m_check })
        n->setInputRegion(&empty);

    SetupText(m_name, 13.f, SkFontStyle::kNormal_Weight, AKTheme::Label);
    m_name.layout().setFlex(1.f);
    m_spinner.setVisible(false);
    m_spinner.stop();
    updateColors();
}

void ControlPanel::NetworkRow::set(const WiFiNetwork &n, bool active, bool connecting) noexcept
{
    network = n;
    m_name.setText(ShortName(n.ssid));
    m_signal.setStrength(n.strength);
    m_lock.setVisible(n.secured);
    m_check.setVisible(active);
    m_spinner.setVisible(connecting);

    if (connecting) m_spinner.start();
    else            m_spinner.stop();

    m_clickable = !active && !connecting;
    updateColors();
}

void ControlPanel::NetworkRow::setAction(const std::string &text) noexcept
{
    for (AKNode *n : std::initializer_list<AKNode*>{ &m_signal, &m_lock, &m_spinner, &m_check })
        n->setVisible(false);

    m_spinner.stop();
    m_name.setText(text);
}

void ControlPanel::NetworkRow::updateColors() noexcept
{
    const CZColorScheme scheme { colorScheme() };
    const bool hover { m_clickable && m_hover };
    setBackgroundColor(m_clickable && m_pressed ? AKTheme::ControlPressedFill.resolve(scheme) :
                       hover ? AKTheme::ControlHoverFill.resolve(scheme) : SK_ColorTRANSPARENT);

    const SkColor label { AKTheme::Label.resolve(scheme) };
    m_lock.setColor(AKTheme::SecondaryLabel.resolve(scheme));
    m_check.setColor(label);
}

void ControlPanel::NetworkRow::pointerEnterEvent(const CZPointerEnterEvent &e)
{
    AKRoundSolidColor::pointerEnterEvent(e);
    m_hover = true;
    updateColors();
}

void ControlPanel::NetworkRow::pointerLeaveEvent(const CZPointerLeaveEvent &e)
{
    AKRoundSolidColor::pointerLeaveEvent(e);
    m_hover = m_pressed = false;
    updateColors();
}

void ControlPanel::NetworkRow::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    AKRoundSolidColor::pointerButtonEvent(e);

    if (e.button != BTN_LEFT || !m_clickable)
        return;

    if (e.pressed)
        m_pressed = true;
    else if (m_pressed)
    {
        m_pressed = false;
        updateColors();

        // Last: it may rebuild the list (destroying this row)
        if (onClicked)
            onClicked();
        return;
    }

    updateColors();
}

void ControlPanel::NetworkRow::colorSchemeEvent(const CZColorSchemeEvent &e)
{
    AKRoundSolidColor::colorSchemeEvent(e);
    updateColors();
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
    layout().setPadding(YGEdgeHorizontal, TilePadding);
    layout().setGap(YGGutterColumn, TileGap);

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

    m_caret.setInputRegion(&empty);
    m_caret.setVisible(false);

    updateColors();
}

void ControlPanel::Tile::setExpandable(bool expandable) noexcept
{
    m_expandable = expandable;
    m_caret.setVisible(expandable);
}

void ControlPanel::Tile::setExpanded(bool expanded) noexcept
{
    m_caret.setIconFromName(expanded ? "expand_less" : "expand_more");
}

bool ControlPanel::Tile::caretHit() const noexcept
{
    // The caret and a margin around it (the tile's right end)
    return m_expandable && AKApp::Get()->pointer().pos().x() >= m_caret.worldRect().x() - TileGap;
}

SkScalar ControlPanel::Tile::contentWidth() const noexcept
{
    // Every state it may show, not just the current one: it never resizes when toggled
    static const SkScalar stateWidth { [] {
        AKText probe { "" };
        SetupText(probe, 11.f, SkFontStyle::kNormal_Weight, AKTheme::SecondaryLabel);
        SkScalar width { 0.f };

        for (const char *state : { "On", "Off", "Unavailable" })
        {
            probe.setText(state);
            width = std::max(width, probe.layout().width().value);
        }

        return width;
    }() };

    const SkScalar titleWidth { m_title.text().empty() ? 0.f : m_title.layout().width().value };
    return 2.f * TilePadding + BadgeSize + TileGap + std::max(titleWidth, stateWidth) +
        (m_expandable ? TileGap + CaretSize : 0.f);
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
    m_caret.setColor(AKTheme::SecondaryLabel.resolve(scheme));
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
    m_hover = m_pressed = m_pressedCaret = false;
    updateColors();
}

void ControlPanel::Tile::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    AKRoundSolidColor::pointerButtonEvent(e);

    if (e.button != BTN_LEFT || !m_available)
        return;

    // The caret: shows (or hides) what it expands, doesn't toggle.
    if (e.pressed && caretHit())
    {
        m_pressedCaret = true;
        return;
    }

    if (!e.pressed && m_pressedCaret)
    {
        m_pressedCaret = false;

        if (caretHit() && onExpand)
            onExpand();
        return;
    }

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

void ControlPanel::RoundButton::setIcon(const char *icon) noexcept
{
    GetFirm()->assets.show(m_icon, LoadIcon(icon));
}

void ControlPanel::RoundButton::updateColors() noexcept
{
    const CZColorScheme scheme { colorScheme() };
    setBackgroundColor((m_pressed ? AKTheme::ControlPressedFill : m_hover ? AKTheme::ControlHoverFill : AKTheme::ControlFill).resolve(scheme));
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
