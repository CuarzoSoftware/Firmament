#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <Marco/Roles/MMenu.h>
#include <AK/Nodes/AKContainer.h>
#include <AK/Nodes/AKRoundSolidColor.h>
#include <AK/Nodes/AKImageFrame.h>
#include <AK/Nodes/AKText.h>
#include <AK/Nodes/AKSlider.h>
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
 * - Toggle tiles: Wi-Fi, Bluetooth, Airplane Mode, Dark Mode, Night Shift and Keyboard Light.
 * - Sliders: display brightness and output volume.
 *
 * Only the volume slider is functional for now (see Status::setVolume()); the other controls
 * just show their look (toggles toggle, the brightness slider slides).
 */
class ControlPanel final : public MMenu
{
public:
    ControlPanel() noexcept;

    /// Opens under @p anchor (the indicators) of the bar @p topbar.
    void openUnder(TopbarSurface *topbar, AKNode *anchor) noexcept;

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
        std::function<void(bool)> onToggled;
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
        bool m_toggled { false }, m_hover { false }, m_pressed { false }, m_available { true };
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

private:
    void refreshSize() noexcept;

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
    Tile *m_wifi {}, *m_bluetooth {}, *m_airplane {}, *m_darkMode {}, *m_nightLight {}, *m_keyboardLight {};
    std::vector<std::unique_ptr<Tile>> m_tileNodes;

    // Slider cards.
    Card m_displayCard { YGFlexDirectionColumn, &m_root };
    AKText m_displayLabel { "Display", &m_displayCard };
    AKSlider m_brightness { &m_displayCard };
    Card m_soundCard { YGFlexDirectionColumn, &m_root };
    AKText m_soundLabel { "Sound", &m_soundCard };
    AKSlider m_volume { &m_soundCard };
};

#endif // CONTROLPANEL_H
