#ifndef TOPBARMENUNODE_H
#define TOPBARMENUNODE_H

#include <AK/Nodes/AKContainer.h>
#include <AK/Nodes/AKText.h>
#include <Core/Types.h>

using namespace CZ;

class TopbarItem final : public AKContainer
{
public:
    TopbarItem(AKNode *parent = nullptr) noexcept;

    void setText(const std::string &text) noexcept;
    void setIcon(std::shared_ptr<RImage> img) noexcept;

    void pointerEnterEvent(const CZPointerEnterEvent &e) override;
    void pointerButtonEvent(const CZPointerButtonEvent &e) override;

    std::shared_ptr<AKText> title;
    std::shared_ptr<AKImageFrame> icon;
};

#endif // TOPBARMENUNODE_H
