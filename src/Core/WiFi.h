#ifndef WIFI_H
#define WIFI_H

#include <CZ/Core/CZObject.h>
#include <CZ/Core/CZSignal.h>
#include <CZ/Core/CZBusWorker.h>
#include <CZ/Core/CZTimer.h>
#include <CZ/Core/CZBus.h>
#include <memory>
#include <string>
#include <vector>

struct sd_bus_slot;

/// A Wi-Fi network in range (its access points grouped by name).
struct WiFiNetwork
{
    std::string ssid;
    CZ::UInt8 strength { 0 };       ///< 0..100, of its strongest access point
    bool secured { false };
    bool enterprise { false };      ///< 802.1X (not supported for joining)
    bool sae { false };             ///< WPA3 only
    bool wep { false };             ///< WEP only (not supported for joining)
    std::string accessPoint;        ///< Its strongest access point (D-Bus path)
    std::string connection;         ///< Its saved profile (D-Bus path), empty if unknown
    std::string uuid;               ///< Its saved profile's UUID
    bool autoJoin { true };         ///< Its saved profile connects automatically

    bool known() const noexcept { return !connection.empty(); }
    std::string securityName() const noexcept;
};

/// NetworkManager's Wi-Fi as shown by the Wi-Fi pane.
struct WiFiState
{
    enum class Link { Disconnected, Connecting, Connected };

    bool loaded { false };          ///< Read at least once
    bool available { false };       ///< NetworkManager runs and there's a Wi-Fi device
    bool enabled { false };
    std::string device;             ///< The Wi-Fi device (D-Bus path)
    std::string interface;
    std::string hwAddress;
    Link link { Link::Disconnected };
    std::string activeSsid;         ///< Connected (or connecting) to
    std::string ipAddress, router;  ///< While connected
    std::vector<WiFiNetwork> networks; ///< In range, by name

    const WiFiNetwork *active() const noexcept;

    /// @cond INTERNAL
    CZ::UInt32 deviceState { 0 }, stateReason { 0 };
    /// @endcond
};

/**
 * @brief NetworkManager's Wi-Fi (system bus): its state, joining and forgetting networks.
 *
 * Everything is read and done off the UI thread (see CZBusWorker); onChanged tells when state()
 * changed. Without NetworkManager (or a Wi-Fi device) state().available is false.
 */
class WiFi final : public CZ::CZObject
{
public:
    WiFi() noexcept;
    ~WiFi() noexcept;

    const WiFiState &state() const noexcept { return m_state; }

    void setEnabled(bool enabled) noexcept;

    /**
     * @brief Asks the device to look for networks (e.g. while the pane is shown).
     *
     * While it's on and no network is listed (e.g. right after turning it on, when the device
     * isn't ready to scan yet), it's asked again every few seconds until some show up.
     */
    void scan() noexcept;

    /// On, but no network is listed yet (e.g. just turned on): looking for them.
    bool searching() const noexcept { return m_state.available && m_state.enabled && m_state.networks.empty(); }

    /**
     * @brief Joins @p network: with its saved profile, or a new one (with @p password if secured).
     *
     * If it fails because of the password, onJoinFailed is emitted (a new profile is deleted).
     */
    void join(const WiFiNetwork &network, const std::string &password = "") noexcept;

    /// Deletes its saved profile.
    void forget(const WiFiNetwork &network) noexcept;

    /// Whether its saved profile connects automatically.
    void setAutoJoin(const WiFiNetwork &network, bool autoJoin) noexcept;

    CZ::CZSignal<> onChanged;

    /// Joining @p ssid failed: wrongPassword if it was rejected (or missing).
    CZ::CZSignal<const std::string& /*ssid*/, bool /*wrongPassword*/> onJoinFailed;

private:
    void refresh() noexcept;
    void apply(WiFiState state) noexcept;

    CZ::CZBusWorker m_worker;
    WiFiState m_state;
    std::unique_ptr<CZ::CZTimer> m_refreshTimer;
    std::unique_ptr<CZ::CZTimer> m_scanRetry; // While on without networks (e.g. just turned on)
    std::shared_ptr<CZ::CZBus> m_system; // Kept: the connection lives while it's held (the match too)
    sd_bus_slot *m_match {};
    bool m_busy { false }, m_again { false };
    bool m_scanPending { false }; // Asked before the device was known

    // A join in progress (see onJoinFailed)
    struct Joining
    {
        std::string ssid;
        std::string newConnection; // Created for it
        CZ::Int64 startMs { 0 };
        bool sawProgress { false };
    };
    std::unique_ptr<Joining> m_joining;
};

#endif // WIFI_H
