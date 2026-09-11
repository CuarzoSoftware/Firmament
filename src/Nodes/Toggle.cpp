#include <Nodes/Toggle.h>
#include <Core/Types.h>
#include <Core/Firmament.h>
#include <Core/Assets.h>
#include <Core/Events/CZPointerButtonEvent.h>
#include <AK/AKTheme.h>

Toggle::Toggle(const std::string &text, AKNode *parent) noexcept :
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

    icon.setImage(GetFirm()->assets.check);
    icon.layout().setWidth(MENU_ICON_SIZE);
    icon.layout().setHeight(MENU_ICON_SIZE);
    icon.setAlignment(CZAlignCenter);
    icon.setSizeMode(AKImageFrame::SizeMode::Contain);
    icon.enableReplaceImageColor(true);
    icon.setColor(AKTheme::Text.light());
    icon.setVisible(false); // hidden until checked
}

Toggle::~Toggle() noexcept {}

void Toggle::setText(const std::string &text) noexcept
{
    this->text.setText(text);
}

void Toggle::setChecked(bool checked) noexcept
{
    m_checked = checked;
    icon.setVisible(checked);
}

void Toggle::setEnabled(bool enabled) noexcept
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    applyColors();
}

void Toggle::setShortcut(const std::string &sc) noexcept
{
    if (sc.empty())
        return shortcut.reset();

    if (shortcut)
        shortcut->setText(sc);
    else
    {
        shortcut = std::make_unique<AKText>(sc, this);
        shortcut->setFontSize(ITEM_FONT_SIZE);
        shortcut->setFontStyle(SkFontStyle(
            SkFontStyle::Weight::kMedium_Weight,
            SkFontStyle::Width::kSemiCondensed_Width,
            SkFontStyle::Slant::kUpright_Slant));
    }

    applyColors();
}

void Toggle::setHover(bool hover) noexcept
{
    if (m_hover == hover)
        return;
    m_hover = hover;
    applyColors();
}

void Toggle::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    MenuItem::pointerButtonEvent(e);

    if (e.button != BTN_LEFT || e.pressed || !m_enabled)
        return;

    if (onClicked)
        onClicked();

    // Dismiss the whole menu chain after activating the item.
    GetFirm()->setActiveTopbarItem(nullptr);
}

void Toggle::applyColors() noexcept
{
    const bool hl { m_hover && m_enabled };

    if (hl)
    {
        text.setTextColor(CZAdaptiveColor(0xFFFFFFFF));
        if (shortcut) shortcut->setTextColor(CZAdaptiveColor(0xAAFFFFFF));
        icon.setColor(0xFFFFFFFF);
    }
    else
    {
        const CZAdaptiveColor label { m_enabled ? AKTheme::Text : AKTheme::TertiaryLabel };
        text.setTextColor(label);
        if (shortcut) shortcut->setTextColor(AKTheme::TertiaryLabel);
        icon.setColor(label.light());
    }
}
