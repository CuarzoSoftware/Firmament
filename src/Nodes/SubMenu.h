#ifndef SUBMENU_H
#define SUBMENU_H

#include <Nodes/MenuItem.h>
#include <AK/Nodes/AKImageFrame.h>
#include <Core/Types.h>
#include <memory>

/**
 * @brief A menu row that opens a child menu (submenu) to its right, shown with a trailing caret.
 *
 * The child popup is set in `submenu`. The owning MenuSurface opens it on hover and keeps this item
 * in the hover state while the submenu is visible.
 */
class SubMenu final : public MenuItem
{
public:
    SubMenu(const std::string &text = "", AKNode *parent = nullptr) noexcept;
    ~SubMenu() noexcept;

    void setText(const std::string &text) noexcept override;
    void setIconName(const std::string &iconName) noexcept override;
    void setEnabled(bool enabled) noexcept override;
    void setHover(bool hover) noexcept override;

    std::unique_ptr<AKImageFrame> icon;
    AKText text;
    AKContainer spacer;
    AKImageFrame caret;

    MenuModel *submenu { nullptr }; ///< Child menu opened when hovered (reparented into a pooled MenuSurface).

private:
    void applyColors() noexcept;
    bool m_hover { false };
    bool m_enabled { true };
};

#endif // SUBMENU_H
