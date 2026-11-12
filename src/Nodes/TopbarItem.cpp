#include <AK/Nodes/AKImageFrame.h>
#include <Ream/RImage.h>
#include <Ream.h>
#include <Core/Events/CZPointerButtonEvent.h>
#include <Nodes/TopbarItem.h>
#include <Surfaces/TopbarSurface.h>
#include <Core/Firmament.h>
#include <AK/AKLog.h>

TopbarItem::TopbarItem(AKNode *parent) noexcept : AKContainer(YGFlexDirectionRow, false, parent)
{
    setInputRegion(nullptr);
    layout().setHeightPercent(100.f);
    layout().setJustifyContent(YGJustifyCenter);
    layout().setAlignItems(YGAlignCenter);
    applyUIScale();
}

void TopbarItem::setUIScale(float scale) noexcept
{
    if (m_uiScale == scale)
        return;

    m_uiScale = scale;
    applyUIScale();
}

void TopbarItem::applyUIScale() noexcept
{
    const float s { m_uiScale };
    layout().setPadding(YGEdgeHorizontal, TOPBAR_ITEM_PADDING * s);

    if (title)
        title->setFontSize(TOPBAR_FONT_SIZE * s);

    if (icon)
    {
        icon->layout().setHeight(TOPBAR_ICON_SIZE * s);
        icon->layout().setWidth((TOPBAR_ICON_SIZE + 3) * s);
    }
}

void TopbarItem::setText(const std::string &text) noexcept
{
    if (!title)
    {
        title.reset(new AKText(text, this));
        title->setFontSize(TOPBAR_FONT_SIZE * m_uiScale);
        title->setFontStyle(SkFontStyle(
            SkFontStyle::Weight::kMedium_Weight,
            SkFontStyle::Width::kNormal_Width,
            SkFontStyle::Slant::kUpright_Slant));
    }
    else
        title->setText(text);
}

void TopbarItem::setIcon(std::shared_ptr<RImage> img) noexcept
{
    if (!icon)
    {
        icon.reset(new AKImageFrame(this));
        icon->setAlignment(CZ::CZAlignCenter);
        icon->setSizeMode(AKImageFrame::SizeMode::Contain);
        icon->layout().setHeight(TOPBAR_ICON_SIZE * m_uiScale);
        icon->layout().setWidth((TOPBAR_ICON_SIZE + 3) * m_uiScale);
    }

    GetFirm()->assets.show(*icon, img); // Sized for each screen's scale
}

void TopbarItem::pointerEnterEvent(const CZPointerEnterEvent &e)
{
    AKContainer::pointerEnterEvent(e);

    if (GetFirm()->activeTopbarItem && GetFirm()->activeTopbarItem != this)
        GetFirm()->setActiveTopbarItem(this);
}

void TopbarItem::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    AKContainer::pointerButtonEvent(e);

    if (e.button == BTN_LEFT && !e.pressed)
        GetFirm()->setActiveTopbarItem(this);
}
