#ifndef ACTION_H
#define ACTION_H

#include <Nodes/MenuItem.h>

class Action final : public MenuItem
{
public:
    Action(const std::string &text, AKNode *parent = nullptr) noexcept;
    ~Action() noexcept;
    void setIcon(const std::string &iconName) noexcept;
    void setHover(bool hover) noexcept override;
    void setShortcut(const std::string &shortcut) noexcept;
    std::unique_ptr<AKImageFrame> icon;
    AKText text;
    AKContainer spacer;
    std::unique_ptr<AKText> shortcut;
};

#endif // ACTION_H
