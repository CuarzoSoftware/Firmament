#include <Core/Types.h>
#include <Core/Firmament.h>
#include <Core/Assets.h>
#include <Nodes/Divider.h>
#include <AK/Nodes/AKThreePatch.h>
#include <AK/Nodes/AKText.h>
#include <AK/AKTheme.h>
#include <AK/AKLog.h>

Divider::Divider(const std::string &title, AKNode *parent) noexcept : MenuItem(parent)
{
    layout().setFlexDirection(YGFlexDirectionColumn);
    layout().setAlignItems(YGAlignFlexStart);
    layout().setJustifyContent(YGJustifyCenter);
    layout().setWidthPercent(100.f);
    layout().setMargin(YGEdgeVertical, 4.f);
    layout().setPadding(YGEdgeHorizontal, 10.f);
    layout().setGap(YGGutterAll, 6.f);
    setTitle(title);

    line.layout().setWidthPercent(100.f);
    line.layout().setHeight(1);
    line.layout().setMargin(YGEdgeAll, 0.f);
    line.setImage(GetFirm()->assets.divider3Patch);
    line.setImageScale(1);
    line.setSideSrcRect(SkRect::MakeXYWH(0, 0, 2, 3));
    line.setCenterSrcRect(SkRect::MakeXYWH(3, 0, 1, 3));
    line.setOpacity(SkScalar(SkColorGetA(AKTheme::TertiaryLabel.light()))/255.f);
    updateLine();
}

Divider::~Divider() noexcept
{

}

void Divider::setHover(bool hover) noexcept
{

}

void Divider::siblingsChanged() noexcept
{
    updateLine();
}

void Divider::pointerEnterEvent(const CZPointerEnterEvent &e)
{
    AKContainer::pointerEnterEvent(e);
}

void Divider::setTitle(const std::string &title) noexcept
{
    if (title.empty())
        return text.reset();

    if (text)
    {
        text->setText(title);
    }
    else
    {
        text = std::make_unique<AKText>(title, this);
        text->setTextColor(AKTheme::TertiaryLabel);
        text->setFontStyle(SkFontStyle::Bold());
        text->setFontSize(9.f);
    }
}

void Divider::updateLine() noexcept
{
    line.setVisible(parent() && parent()->children().front() != this && parent()->children().back() != this);
}
