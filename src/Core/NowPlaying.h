#ifndef NOWPLAYING_H
#define NOWPLAYING_H

#include <Core/Types.h>
#include <CZ/Core/CZObject.h>
#include <CZ/Core/CZSignal.h>
#include <CZ/Core/CZBusWorker.h>
#include <CZ/Core/CZTimer.h>
#include <CZ/Core/CZBus.h>
#include <memory>
#include <string>

struct sd_bus_slot;

using namespace CZ;

/**
 * @brief What's playing (MPRIS media players: Spotify, browsers, video players...), for the
 *        control panel's Now Playing card.
 *
 * The player shown is the one playing, else the last one that did (or any). Its artwork is a local
 * file or downloaded once to `~/.cache/cz-firmament/artwork/` (players like Spotify give URLs).
 * Kept up to date from the players' D-Bus signals; reads and commands run off the UI thread.
 */
class NowPlaying final : public CZObject
{
public:
    NowPlaying() noexcept;
    ~NowPlaying() noexcept;

    struct State
    {
        bool available { false };   ///< Some player is there
        std::string player;         ///< Its bus name
        std::string identity;       ///< e.g. "Spotify"
        std::string title, artist;
        std::string artwork;        ///< Local file (empty: none or not downloaded yet)
        bool playing { false };
        bool canPrevious { false }, canNext { false }, canPlayPause { false }, canRaise { false };
    };

    const State &state() const noexcept { return m_state; }

    void playPause() noexcept;
    void previous() noexcept;
    void next() noexcept;

    /// Brings the player's window up.
    void raise() noexcept;

    CZSignal<> onChanged;

private:
    void refresh() noexcept;
    void call(const char *method, bool root = false) noexcept;

    CZBusWorker m_worker { CZBusWorker::User, 1000 };
    std::shared_ptr<CZBus> m_bus;
    sd_bus_slot *m_propertiesMatch {}, *m_ownerMatch {};
    std::unique_ptr<CZTimer> m_refreshTimer;
    State m_state;
    std::string m_lastPlaying; // The player that played last
    bool m_busy { false }, m_again { false };
};

#endif // NOWPLAYING_H
