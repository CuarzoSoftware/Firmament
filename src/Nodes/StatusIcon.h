#ifndef STATUSICON_H
#define STATUSICON_H

#include <AK/Nodes/AKImageFrame.h>
#include <Core/Types.h>
#include <string>

using namespace CZ;

/**
 * @brief A status indicator in the bar's right corner (network, volume, battery).
 *
 * Shows a symbolic theme icon tinted like the bar's text, hidden while it has no icon name. Not
 * interactive: it takes no input (clicks go to the bar).
 */
class StatusIcon final : public AKImageFrame
{
public:
    StatusIcon(AKNode *parent = nullptr) noexcept;

    /// The symbolic icon to show (e.g. "battery-level-80-symbolic"), empty to hide it.
    void setIconName(const std::string &name) noexcept;

    /// Scales the icon and its spacing (see Firmament::uiScale()).
    void setUIScale(float scale) noexcept;

private:
    std::string m_name;
};

#endif // STATUSICON_H
