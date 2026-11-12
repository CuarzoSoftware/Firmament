#include <Nodes/Toggle.h>
#include <Surfaces/MenuSurface.h>
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
    this->text.setFontStyle(SkFontStyle(
        SkFontStyle::Weight::kMedium_Weight,
        SkFontStyle::Width::kNormal_Width,
        SkFontStyle::Slant::kUpright_Slant));
    spacer.layout().setFlex(10.f);

    GetFirm()->assets.show(icon, GetFirm()->assets.check);
    icon.setAlignment(CZAlignCenter);
    icon.setSizeMode(AKImageFrame::SizeMode::Contain);
    icon.enableReplaceImageColor(true);
    icon.setColor(AKTheme::Text.light());
    icon.setOpacity(0.f); // Invisible until checked, but its space stays (titles keep aligned)
    applyUIScale();
}

void Toggle::applyUIScale() noexcept
{
    MenuItem::applyUIScale();
    const float k { m_uiScale };
    layout().setPadding(YGEdgeTop, 4.f * k);
    layout().setPadding(YGEdgeBottom, 5.f * k);
    layout().setPadding(YGEdgeHorizontal, 10.f * k);
    text.setFontSize(ITEM_FONT_SIZE * k);
    icon.layout().setWidth(MENU_ICON_SIZE * k);
    icon.layout().setHeight(MENU_ICON_SIZE * k);

    if (shortcut)
        shortcut->setFontSize(ITEM_FONT_SIZE * k);
}

Toggle::~Toggle() noexcept {}

void Toggle::setText(const std::string &text) noexcept
{
    this->text.setText(text);
}

void Toggle::setChecked(bool checked) noexcept
{
    m_checked = checked;
    icon.setOpacity(checked ? 1.f : 0.f); // The check mark's space stays when unchecked
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
        shortcut->setFontSize(ITEM_FONT_SIZE * m_uiScale);
        shortcut->setFontStyle(SkFontStyle(
            SkFontStyle::Weight::kMedium_Weight,
            SkFontStyle::Width::kNormal_Width,
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

    // Blinks, then runs onClicked and dismisses the whole menu chain (see MenuSurface::activate()).
    MenuSurface::Activate(*this);
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
