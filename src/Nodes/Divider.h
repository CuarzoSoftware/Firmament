#ifndef DIVIDER_H
#define DIVIDER_H

#include <Nodes/MenuItem.h>
#include <AK/Nodes/AKThreePatch.h>

using namespace CZ;

class Divider : public MenuItem
{
public:
    Divider(const std::string &title = "", AKNode *parent = nullptr) noexcept;
    ~Divider() noexcept;
    void setHover(bool hover) noexcept override;
    void siblingsChanged() noexcept override;
    void setText(const std::string &title) noexcept override { setTitle(title); }
    void pointerEnterEvent(const CZPointerEnterEvent &e) override;
    void setTitle(const std::string &title) noexcept;
    void updateLine() noexcept;
    AKThreePatch line { CZOrientation::H, this };
    std::unique_ptr<AKText> text;
};

#endif // DIVIDER_H
