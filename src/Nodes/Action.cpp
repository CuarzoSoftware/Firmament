#include <Nodes/Action.h>
#include <Core/Types.h>
#include <Core/Firmament.h>
#include <AK/AKTheme.h>
#include <AK/Nodes/AKImageFrame.h>
#include <Ream/RImage.h>
#include <AK/AKLog.h>

Action::Action(const std::string &text, AKNode *parent) noexcept :
    MenuItem(parent),
    text(text, this),
    spacer(YGFlexDirectionColumn, false, this)
{
    layout().setPadding(YGEdgeTop, 4.f);
    layout().setPadding(YGEdgeBottom, 5.f);
    layout().setPadding(YGEdgeHorizontal, 10.f);
    this->text.setFontSize(ITEM_FONT_SIZE);
    this->text.setFontStyle(SkFontStyle(
        SkFontStyle::Weight::kMedium_Weight,
        SkFontStyle::Width::kSemiCondensed_Width,
        SkFontStyle::Slant::kUpright_Slant));
    spacer.layout().setFlex(10.f);
}

Action::~Action() noexcept
{

}

void Action::setIcon(const std::string &iconName) noexcept
{
    auto *f { GetFirm() };
    const auto *iconInfo { f->xdgkit->iconThemeManager().findIcon(iconName, MENU_ICON_SIZE, 2, XDGIcon::PNG | XDGIcon::SVG, { DEFAULT_ICON_THEME, "" }) };

    if (!iconInfo)
    {
        AKLog(CZFatal, "NO ICON");
        return icon.reset();
    }

    std::shared_ptr<RImage> img;

    const Int32 pixelSize = MENU_ICON_SIZE * 2;

    if ((iconInfo->extensions() & XDGIcon::SVG))
        img = RImage::LoadFile(iconInfo->getPath(XDGIcon::SVG), f->safeFormat, {pixelSize, pixelSize});

    if (!img && (iconInfo->extensions() & XDGIcon::SVG))
        img = RImage::LoadFile(iconInfo->getPath(XDGIcon::PNG), f->safeFormat, {pixelSize, pixelSize});

    if (!img)
        return icon.reset();

    if (icon)
        icon->setImage(img);
    else
    {
        icon = std::make_unique<AKImageFrame>(img);
        icon->layout().setWidth(MENU_ICON_SIZE);
        icon->layout().setHeight(MENU_ICON_SIZE);
        icon->setAlignment(CZAlignCenter);
        icon->setSizeMode(AKImageFrame::SizeMode::Contain);
        icon->setColor(AKTheme::Text.light());
        icon->enableReplaceImageColor(true);
        icon->insertBefore(&text);
    }
}

void Action::setHover(bool hover) noexcept
{
    if (hover)
    {
        text.setTextColor(CZAdaptiveColor(0xFFFFFFFF));

        if (shortcut)
            shortcut->setTextColor(CZAdaptiveColor(0xAAFFFFFF));

        if (icon)
            icon->setColor(0xFFFFFFFF);
    }
    else
    {
        text.setTextColor(AKTheme::Text);

        if (shortcut)
            shortcut->setTextColor(AKTheme::TertiaryLabel);

        if (icon)
            icon->setColor(AKTheme::Text.light());
    }
}

void Action::setShortcut(const std::string &sc) noexcept
{
    if (sc.empty())
        return shortcut.reset();

    if (shortcut)
        shortcut->setText(sc);
    else
    {
        shortcut = std::make_unique<AKText>(sc, this);
        shortcut->setTextColor(AKTheme::TertiaryLabel);
        shortcut->setFontSize(ITEM_FONT_SIZE);
        shortcut->setFontStyle(SkFontStyle(
            SkFontStyle::Weight::kMedium_Weight,
            SkFontStyle::Width::kSemiCondensed_Width,
            SkFontStyle::Slant::kUpright_Slant));
    }
}
