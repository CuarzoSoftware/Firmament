#ifndef TOGGLE_H
#define TOGGLE_H

#include <Nodes/MenuItem.h>
#include <AK/Nodes/AKImageFrame.h>
#include <memory>

/**
 * @brief A menu row with a checked/unchecked state, shown as a leading check mark.
 */
class Toggle final : public MenuItem
{
public:
    Toggle(const std::string &text = "", AKNode *parent = nullptr) noexcept;
    ~Toggle() noexcept;

    void setText(const std::string &text) noexcept override;
    void setShortcut(const std::string &shortcut) noexcept override;
    void setEnabled(bool enabled) noexcept override;
    void setChecked(bool checked) noexcept override;
    void setHover(bool hover) noexcept override;

    void pointerButtonEvent(const CZPointerButtonEvent &e) override;

    AKImageFrame icon { this }; ///< Check mark; visible only while checked.
    AKText text;
    AKContainer spacer;
    std::unique_ptr<AKText> shortcut;

private:
    void applyColors() noexcept;
    bool m_hover { false };
    bool m_enabled { true };
    bool m_checked { false };
};

#endif // TOGGLE_H
