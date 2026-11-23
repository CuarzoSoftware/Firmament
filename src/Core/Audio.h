#ifndef AUDIO_H
#define AUDIO_H

#include <Core/Types.h>
#include <CZ/Core/CZObject.h>
#include <CZ/Core/CZSignal.h>
#include <CZ/Core/CZBusWorker.h>
#include <CZ/Core/CZTimer.h>
#include <CZ/Core/CZEventSource.h>
#include <memory>
#include <string>
#include <vector>
#include <sys/types.h>

using namespace CZ;

/// An audio output (sink) or input (source).
struct AudioDevice
{
    std::string name;        ///< PipeWire/PulseAudio name (what pactl takes)
    std::string displayName; ///< Shown: e.g. "Speaker" for built-in ones, the product otherwise
    std::string type;        ///< "Built-in", "USB", "Bluetooth", "HDMI" or "Virtual"
    float volume { 0.f };    ///< 0..1
    bool muted { false };
    std::vector<std::string> channels; ///< Its channel map (e.g. front-left, front-right, aux0...)

    /// @cond INTERNAL
    std::string remapMaster;            // One of ours (see Audio::select()): the device it routes to
    std::vector<UInt32> remapChannels;  // and which of its channels
    /// @endcond
};

/// The default output (or input): a device, optionally just some of its channels.
struct AudioSelection
{
    std::string device;             ///< Name
    std::vector<UInt32> channels;   ///< 0-based, empty: all of them
    float volume { -1.f };          ///< 0..1, -1 if none
    bool muted { false };
    bool valid { false };
    std::string node;               ///< The default node (the device, or the remap)
};

struct AudioState
{
    bool loaded { false };
    bool available { false };       ///< pactl works (PipeWire or PulseAudio)
    std::vector<AudioDevice> outputs, inputs;
    AudioSelection output, input;   ///< The default ones

    const AudioDevice *find(bool input, const std::string &name) const noexcept;

    /// "Channels 1+2", "Channel 3"... (empty for all of them).
    static std::string ChannelsName(bool input, const std::vector<UInt32> &channels) noexcept;
};

/**
 * @brief The audio devices for the control panel: choosing the output and input (and which of their
 *        channels), and their volume.
 *
 * Through `pactl` (PipeWire or PulseAudio), off the UI thread; changes from anywhere show up
 * (`pactl subscribe`). Some channels of a device (e.g. outputs 3+4 of a multichannel interface, or
 * one input of it) are used through a virtual device routing to them (module-remap-sink/source),
 * made the default one. One per direction, remembered and recreated on start if missing.
 */
class Audio final : public CZObject
{
public:
    Audio() noexcept;
    ~Audio() noexcept;

    const AudioState &state() const noexcept { return m_state; }

    /// Makes @p device the default output (or input), only @p channels of it if not empty.
    void select(bool input, const std::string &device, std::vector<UInt32> channels = {}) noexcept;

    /// The default output's (input's) volume, muted at 0. Coalesced.
    void setVolume(bool input, float volume) noexcept;

    CZSignal<> onChanged;

private:
    void refresh() noexcept;
    void restoreSaved() noexcept;
    void save() noexcept;
    void applyVolume() noexcept;
    void startMonitor() noexcept;
    void stopMonitor() noexcept;

    struct Saved { std::string device; std::vector<UInt32> channels; };
    Saved m_savedOutput, m_savedInput;
    bool m_restored { false };

    CZBusWorker m_worker { CZBusWorker::User };
    AudioState m_state;
    bool m_busy { false }, m_again { false };
    std::unique_ptr<CZTimer> m_refreshTimer, m_retryTimer;
    float m_pendingVolume[2] { -1.f, -1.f }; // Output, input
    bool m_volumeBusy { false };

    // pactl subscribe
    pid_t m_monitorPid { -1 };
    std::shared_ptr<CZEventSource> m_monitorSource;
    std::string m_monitorBuffer;
};

#endif // AUDIO_H
