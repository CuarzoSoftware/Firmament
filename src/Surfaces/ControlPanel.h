#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <Marco/Roles/MMenu.h>
#include <AK/Nodes/AKContainer.h>
#include <AK/Nodes/AKRoundSolidColor.h>
#include <AK/Nodes/AKImageFrame.h>
#include <AK/Nodes/AKText.h>
#include <AK/Nodes/AKSlider.h>
#include <AK/Nodes/AKFontIcon.h>
#include <AK/Nodes/AKWiFiIcon.h>
#include <AK/Nodes/AKActivityIndicator.h>
#include <AK/Nodes/AKTextInput.h>
#include <AK/Nodes/AKButton.h>
#include <AK/Nodes/AKScroll.h>
#include <CZ/Services/SWiFi.h>
#include <Core/Audio.h>
#include <Core/NowPlaying.h>
#include <AK/Nodes/AKImage.h>
#include <Core/CZTimer.h>
#include <AK/Effects/AKBackgroundBoxShadowEffect.h>
#include <Core/Types.h>
#include <functional>
#include <memory>
#include <vector>

using namespace CZ;

/**
 * @brief The control panel, like macOS Control Center / GNOME's quick settings.
 *
 * Opened by pressing the bar's status indicators (see Firmament::toggleControlPanel()), placed like
 * the bar's menus (from the indicators to the right), but kept 4 pt away from the bar and the
 * screen's right edge. A rounder popup whose controls are grouped into translucent cards:
 * - Header: the battery level, and round buttons for system settings, locking the session and
 *   powering off.
 * - Toggle tiles: Wi-Fi, Bluetooth, High Performance (off: energy
 *   saving), Dark Mode, Night Shift and Keyboard Light.
 * - The Wi-Fi tile's caret (at its right) shows the networks in range right under its row (a
 *   scrollable list with a maximum height): pressing one joins it (asking for its password inline if needed). See SWiFi.
 * - Sliders: display brightness, output volume and microphone level. The sound ones end with a
 *   button opening a menu: Advanced Settings… (System Settings' Sound pane), then the outputs
 *   (inputs) to choose from, and the channels of multichannel ones (pairs for outputs, single
 *   ones for inputs), see Audio.
 * - Now Playing (when a media player is there, see NowPlaying): the artwork, title and artist
 *   (clicking them brings the player up) and previous / play-pause / next.
 *
 * Only the volume slider is functional for now (see Status::setVolume()); the other controls
 * just show their look (toggles toggle, the brightness slider slides).
 */
class ControlPanel final : public MMenu
{
public:
    ControlPanel() noexcept;

    /// Opens under @p anchor (the indicators) of the bar @p topbar, grabbing with @p grab if set.
    void openUnder(TopbarSurface *topbar, AKNode *anchor, const CZInputEvent *grab = nullptr) noexcept;

    /// Shows Firmament's current Status (battery, volume, Wi-Fi).
    void updateStatus() noexcept;

    // A rounded tile with a round icon badge, a title and its state (On / Off).
    class Tile final : public AKRoundSolidColor
    {
    public:
        Tile(const std::string &title, const char *icon, AKNode *parent) noexcept;
        void setToggled(bool toggled) noexcept;

        /// Unavailable (e.g. no Bluetooth adapter): dimmed, clicks ignored.
        void setAvailable(bool available) noexcept;
        bool toggled() const noexcept { return m_toggled; }

        /// Shows a caret at its right: pressing it calls onExpand instead of toggling.
        void setExpandable(bool expandable) noexcept;

        /// The caret points up while what it shows is shown.
        void setExpanded(bool expanded) noexcept;

        /// The width its content needs: padding, badge, its title or longest state (and caret).
        SkScalar contentWidth() const noexcept;
        std::function<void(bool)> onToggled;
        std::function<void()> onExpand;
    protected:
        void pointerEnterEvent(const CZPointerEnterEvent &e) override;
        void pointerLeaveEvent(const CZPointerLeaveEvent &e) override;
        void pointerButtonEvent(const CZPointerButtonEvent &e) override;
        void colorSchemeEvent(const CZColorSchemeEvent &e) override;
    private:
        void updateColors() noexcept;
        AKRoundSolidColor m_badge;
        AKImageFrame m_icon { &m_badge };
        AKContainer m_texts { YGFlexDirectionColumn, false, this };
        AKText m_title { "", &m_texts };
        AKText m_state { "", &m_texts };
        AKFontIcon m_caret { "expand_more", 18, this };
        bool caretHit() const noexcept;
        bool m_toggled { false }, m_hover { false }, m_pressed { false }, m_available { true };
        bool m_expandable { false }, m_pressedCaret { false };
    };

    // A Wi-Fi network in the list (or, with setAction(), a plain text row like "Wi-Fi Settings…").
    class NetworkRow final : public AKRoundSolidColor
    {
    public:
        NetworkRow(AKNode *parent) noexcept;
        void set(const SWiFiNetwork &network, bool active, bool connecting) noexcept;
        void setAction(const std::string &text) noexcept;
        SWiFiNetwork network;
        std::function<void()> onClicked;
    protected:
        void pointerEnterEvent(const CZPointerEnterEvent &e) override;
        void pointerLeaveEvent(const CZPointerLeaveEvent &e) override;
        void pointerButtonEvent(const CZPointerButtonEvent &e) override;
        void colorSchemeEvent(const CZColorSchemeEvent &e) override;
    private:
        void updateColors() noexcept;
        AKWiFiIcon m_signal { 100, 16, this };
        AKText m_name { "", this };
        AKFontIcon m_lock { "lock", 13, this };
        AKActivityIndicator m_spinner { 14, this };
        AKFontIcon m_check { "check", 16, this };
        bool m_hover { false }, m_pressed { false }, m_clickable { true };
    };

    // A rounded translucent white group with a soft shadow.
    class Card final : public AKRoundSolidColor
    {
    public:
        Card(YGFlexDirection direction, AKNode *parent) noexcept;
    protected:
        void colorSchemeEvent(const CZColorSchemeEvent &e) override;
    private:
        void updateColors() noexcept;
        AKBackgroundBoxShadowEffect m_shadow;
    };

    // A round icon button (header actions).
    class RoundButton final : public AKRoundSolidColor
    {
    public:
        RoundButton(const char *icon, AKNode *parent) noexcept;
        void setIcon(const char *icon) noexcept;
        std::function<void()> onClicked;
    protected:
        void pointerEnterEvent(const CZPointerEnterEvent &e) override;
        void pointerLeaveEvent(const CZPointerLeaveEvent &e) override;
        void pointerButtonEvent(const CZPointerButtonEvent &e) override;
        void colorSchemeEvent(const CZColorSchemeEvent &e) override;
    private:
        void updateColors() noexcept;
        AKImageFrame m_icon { this };
        bool m_hover { false }, m_pressed { false };
    };

    // Clickable area (the Now Playing artwork and texts)
    class ClickArea final : public AKContainer
    {
    public:
        ClickArea(YGFlexDirection direction, AKNode *parent) noexcept : AKContainer(direction, false, parent) {}
        std::function<void()> onClicked;
    protected:
        void pointerButtonEvent(const CZPointerButtonEvent &e) override;
    private:
        bool m_pressed { false };
    };

private:
    void refreshSize() noexcept;

    /// The Now Playing card (see NowPlaying).
    void updateNowPlaying() noexcept;

    // Audio (output / input)
    void updateAudio() noexcept;

    /// The menu of the outputs (inputs) and their channels, under its button.
    void openAudioMenu(bool input) noexcept;

    // The Wi-Fi networks list
    void setWiFiExpanded(bool expanded) noexcept;
    void updateWiFiList() noexcept;
    void joinNetwork(const SWiFiNetwork &network) noexcept;
    void askPassword(const SWiFiNetwork &network, bool wrong) noexcept;
    void hidePassword() noexcept;
    void joinWithPassword() noexcept;
    void showWiFiMessage(const std::string &message) noexcept; ///< For a few seconds

    /// Closes the panel, then runs @p action (e.g. locking).
    void closeAndRun(std::function<void()> action) noexcept;

    AKContainer m_root { YGFlexDirectionColumn, false, this };

    // Header card: battery | settings, lock, power (+ the power options)
    Card m_headerCard { YGFlexDirectionColumn, &m_root };
    AKContainer m_header { YGFlexDirectionRow, false, &m_headerCard };
    AKImageFrame m_batteryIcon { &m_header };
    AKText m_batteryText { "", &m_header };
    AKContainer m_headerSpacer { YGFlexDirectionRow, false, &m_header };
    RoundButton m_settings { "emblem-system-symbolic", &m_header };
    RoundButton m_lock { "system-lock-screen-symbolic", &m_header };
    RoundButton m_power { "system-shutdown-symbolic", &m_header };

    // Power options, shown by the power button: Sleep, Restart, Log Out, Shut Down.
    AKContainer m_powerRow { YGFlexDirectionRow, false, &m_headerCard };
    std::vector<std::unique_ptr<AKContainer>> m_powerOptions;
    void addPowerOption(const char *icon, const char *label, std::function<void()> action) noexcept;
    void setPowerOptionsVisible(bool visible) noexcept;

    // Toggle tiles card, two per row.
    Card m_tiles { YGFlexDirectionColumn, &m_root };
    std::vector<std::unique_ptr<AKContainer>> m_tileRows;
    Tile *m_wifi {}, *m_bluetooth {}, *m_performance {}, *m_darkMode {}, *m_nightLight {}, *m_keyboardLight {};
    std::vector<std::unique_ptr<Tile>> m_tileNodes;

    // Wi-Fi networks, shown by the Wi-Fi tile's caret (moved into the tiles card, under its row).
    AKContainer m_wifiCard { YGFlexDirectionColumn, false, &m_root };
    AKContainer m_wifiHeader { YGFlexDirectionRow, false, &m_wifiCard };
    AKText m_wifiTitle { "Wi-Fi Networks", &m_wifiHeader };
    AKContainer m_wifiHeaderSpacer { YGFlexDirectionRow, false, &m_wifiHeader };
    AKActivityIndicator m_wifiSpinner { 14, &m_wifiHeader };
    AKText m_wifiMessage { "", &m_wifiCard };   // Off, searching, or why it couldn't join
    AKScroll m_networkScroll { &m_wifiCard };   // Up to MaxListHeight, then scrolls
    AKContainer m_networkList { YGFlexDirectionColumn, false, &m_networkScroll };
    AKContainer m_passwordBox { YGFlexDirectionColumn, false, &m_wifiCard };
    AKText m_passwordLabel { "", &m_passwordBox };
    AKContainer m_passwordRow { YGFlexDirectionRow, false, &m_passwordBox };
    AKTextInput m_password { "Password", &m_passwordRow };
    AKButton m_joinButton { "Join", &m_passwordRow };
    NetworkRow m_wifiSettings { &m_wifiCard };
    std::vector<std::unique_ptr<NetworkRow>> m_networkRows;
    std::unique_ptr<SWiFi> m_wifiBackend;         // Created the first time the list is shown
    SWiFiNetwork m_passwordNetwork;               // Asked for (m_passwordBox shown)
    std::unique_ptr<CZTimer> m_scanTimer, m_messageTimer;
    std::string m_message;
    bool m_wifiExpanded { false };

    // Slider cards.
    Card m_displayCard { YGFlexDirectionColumn, &m_root };
    AKText m_displayLabel { "Display", &m_displayCard };
    AKSlider m_brightness { &m_displayCard };
    Card m_soundCard { YGFlexDirectionColumn, &m_root };
    AKText m_soundLabel { "Sound", &m_soundCard };
    AKContainer m_volumeRow { YGFlexDirectionRow, false, &m_soundCard };
    AKSlider m_volume { &m_volumeRow };
    RoundButton m_outputButton { "pan-down-symbolic", &m_volumeRow };
    Card m_micCard { YGFlexDirectionColumn, &m_root };
    AKText m_micLabel { "Microphone", &m_micCard };
    AKContainer m_micRow { YGFlexDirectionRow, false, &m_micCard };
    AKSlider m_micVolume { &m_micRow };
    RoundButton m_inputButton { "pan-down-symbolic", &m_micRow };
    std::unique_ptr<Audio> m_audio;
    std::unique_ptr<MMenu> m_audioMenu; // Child popup, see openAudioMenu()

    // Now Playing card: [artwork][title / artist][previous play next]
    Card m_nowCard { YGFlexDirectionRow, &m_root };
    ClickArea m_nowInfo { YGFlexDirectionRow, &m_nowCard };
    AKRoundSolidColor m_artFrame { 0, CZBorderRadius::Make(8), &m_nowInfo };
    AKImageFrame m_artPlaceholder { &m_artFrame };
    AKImage m_artwork { &m_artFrame };
    AKContainer m_nowTexts { YGFlexDirectionColumn, false, &m_nowInfo };
    AKText m_nowTitle { "", &m_nowTexts };
    AKText m_nowArtist { "", &m_nowTexts };
    RoundButton m_previous { "media-skip-backward-symbolic", &m_nowCard };
    RoundButton m_playPause { "media-playback-start-symbolic", &m_nowCard };
    RoundButton m_next { "media-skip-forward-symbolic", &m_nowCard };
    std::unique_ptr<NowPlaying> m_nowPlaying;
    std::string m_artworkPath;   // Shown
};

#endif // CONTROLPANEL_H
