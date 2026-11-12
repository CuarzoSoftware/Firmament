#include <Nodes/SubMenu.h>
#include <Core/Types.h>
#include <Core/Firmament.h>
#include <Core/Assets.h>
#include <AK/AKTheme.h>

SubMenu::SubMenu(const std::string &text, AKNode *parent) noexcept :
    MenuItem(parent),
    text(text, this),
    spacer(YGFlexDirectionColumn, false, this),
    caret(this)
{
    this->text.setFontStyle(SkFontStyle(
        SkFontStyle::Weight::kMedium_Weight,
        SkFontStyle::Width::kNormal_Width,
        SkFontStyle::Slant::kUpright_Slant));
    spacer.layout().setFlex(10.f);

    caret.enableReplaceImageColor(true);
    caret.setSizeMode(AKImageFrame::SizeMode::Contain);
    caret.setAlignment(CZAlignCenter);
    GetFirm()->assets.show(caret, GetFirm()->assets.caret);
    caret.setColor(AKTheme::TertiaryLabel.light());
    applyUIScale();
}

void SubMenu::applyUIScale() noexcept
{
    MenuItem::applyUIScale();
    const float k { m_uiScale };
    layout().setPadding(YGEdgeTop, 4.f * k);
    layout().setPadding(YGEdgeBottom, 5.f * k);
    layout().setPadding(YGEdgeHorizontal, 10.f * k);
    text.setFontSize(ITEM_FONT_SIZE * k);
    caret.layout().setWidth(MENU_ICON_SIZE * k);
    caret.layout().setHeight(MENU_ICON_SIZE * k);

    if (icon)
    {
        icon->layout().setWidth(MENU_ICON_SIZE * k);
        icon->layout().setHeight(MENU_ICON_SIZE * k);
    }
}

SubMenu::~SubMenu() noexcept {}

void SubMenu::setText(const std::string &text) noexcept
{
    this->text.setText(text);
}

void SubMenu::setIconName(const std::string &iconName) noexcept
{
    if (iconName.empty())
        return icon.reset();

    auto img { GetFirm()->assets.loadIcon(iconName, Int32(MENU_ICON_SIZE * 2 * TOPBAR_MAX_SCALE)) };

    if (!img)
        return icon.reset();

    if (!icon)
    {
        icon = std::make_unique<AKImageFrame>();
        icon->layout().setWidth(MENU_ICON_SIZE * m_uiScale);
        icon->layout().setHeight(MENU_ICON_SIZE * m_uiScale);
        icon->setAlignment(CZAlignCenter);
        icon->setSizeMode(AKImageFrame::SizeMode::Contain);
        icon->enableReplaceImageColor(true);
        icon->insertBefore(&text);
    }

    GetFirm()->assets.show(*icon, img); // Sized for each screen's scale

    applyColors();
}

void SubMenu::setEnabled(bool enabled) noexcept
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    applyColors();
}

void SubMenu::setHover(bool hover) noexcept
{
    if (m_hover == hover)
        return;
    m_hover = hover;
    applyColors();
}

void SubMenu::applyColors() noexcept
{
    const bool hl { m_hover && m_enabled };

    if (hl)
    {
        text.setTextColor(CZAdaptiveColor(0xFFFFFFFF));
        caret.setColor(0xFFFFFFFF);
        if (icon) icon->setColor(0xFFFFFFFF);
    }
    else
    {
        const CZAdaptiveColor label { m_enabled ? AKTheme::Text : AKTheme::TertiaryLabel };
        text.setTextColor(label);
        caret.setColor(AKTheme::TertiaryLabel.light());
        if (icon) icon->setColor(label.light());
    }
}
