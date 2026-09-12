#ifndef ACTION_H
#define ACTION_H

#include <Nodes/MenuItem.h>
#include <AK/Nodes/AKImageFrame.h>
#include <memory>

/**
 * @brief A clickable menu row: optional leading icon, title and optional trailing shortcut.
 */
class Action final : public MenuItem
{
public:
    Action(const std::string &text = "", AKNode *parent = nullptr) noexcept;
    ~Action() noexcept;

    void setText(const std::string &text) noexcept override;
    void setIconName(const std::string &iconName) noexcept override;
    void setShortcut(const std::string &shortcut) noexcept override;
    void setEnabled(bool enabled) noexcept override;
    bool enabled() const noexcept override { return m_enabled; }
    void setHover(bool hover) noexcept override;

    void pointerButtonEvent(const CZPointerButtonEvent &e) override;

    std::unique_ptr<AKImageFrame> icon;
    AKText text;
    AKContainer spacer;
    std::unique_ptr<AKText> shortcut;

private:
    void applyColors() noexcept;
    bool m_hover { false };
    bool m_enabled { true };
};

#endif // ACTION_H
