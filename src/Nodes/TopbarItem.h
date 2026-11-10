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

    /// Scales the padding, font and icon (see Firmament::uiScale()).
    void setUIScale(float scale) noexcept;
    float uiScale() const noexcept { return m_uiScale; }

    void pointerEnterEvent(const CZPointerEnterEvent &e) override;
    void pointerButtonEvent(const CZPointerButtonEvent &e) override;

    std::shared_ptr<AKText> title;
    std::shared_ptr<AKImageFrame> icon;

private:
    void applyUIScale() noexcept;
    float m_uiScale { 1.f };
public:

    /// The menu this item opens when activated (a topbar menu, or the system menu for the logo).
    /// Firmament reparents this model's content into one of its two pooled MenuSurfaces.
    MenuModel *menu { nullptr };
};

#endif // TOPBARMENUNODE_H
