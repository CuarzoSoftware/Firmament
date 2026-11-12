#include <Nodes/Action.h>
#include <Surfaces/MenuSurface.h>
#include <Core/Types.h>
#include <Core/Firmament.h>
#include <Core/Assets.h>
#include <Core/Events/CZPointerButtonEvent.h>
#include <AK/AKTheme.h>
#include <Ream/RImage.h>

Action::Action(const std::string &text, AKNode *parent) noexcept :
    MenuItem(parent),
    text(text, this),
    spacer(YGFlexDirectionColumn, false, this)
{
    this->text.setFontStyle(SkFontStyle(
        SkFontStyle::Weight::kMedium_Weight,
        SkFontStyle::Width::kNormal_Width,
        SkFontStyle::Slant::kUpright_Slant));
    spacer.layout().setFlex(10.f);
    applyUIScale();
}

void Action::applyUIScale() noexcept
{
    MenuItem::applyUIScale();
    const float k { m_uiScale };
    layout().setPadding(YGEdgeTop, 4.f * k);
    layout().setPadding(YGEdgeBottom, 5.f * k);
    layout().setPadding(YGEdgeHorizontal, 10.f * k);
    text.setFontSize(ITEM_FONT_SIZE * k);
    if (icon)
    {
        icon->layout().setWidth(MENU_ICON_SIZE * k);
        icon->layout().setHeight(MENU_ICON_SIZE * k);
    }

    if (shortcut)
        shortcut->setFontSize(ITEM_FONT_SIZE * k);
}

Action::~Action() noexcept {}

void Action::setText(const std::string &text) noexcept
{
    this->text.setText(text);
}

void Action::setIconName(const std::string &iconName) noexcept
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

void Action::setShortcut(const std::string &sc) noexcept
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

void Action::setEnabled(bool enabled) noexcept
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    applyColors();
}

void Action::setHover(bool hover) noexcept
{
    if (m_hover == hover)
        return;
    m_hover = hover;
    applyColors();
}

void Action::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    MenuItem::pointerButtonEvent(e);

    if (e.button != BTN_LEFT || e.pressed || !m_enabled)
        return;

    // Blinks, then runs onClicked and dismisses the whole menu chain (see MenuSurface::activate()).
    MenuSurface::Activate(*this);
}

void Action::applyColors() noexcept
{
    const bool hl { m_hover && m_enabled };

    if (hl)
    {
        text.setTextColor(CZAdaptiveColor(0xFFFFFFFF));
        if (shortcut) shortcut->setTextColor(CZAdaptiveColor(0xAAFFFFFF));
        if (icon) icon->setColor(0xFFFFFFFF);
    }
    else
    {
        const CZAdaptiveColor label { m_enabled ? AKTheme::Text : AKTheme::TertiaryLabel };
        text.setTextColor(label);
        if (shortcut) shortcut->setTextColor(AKTheme::TertiaryLabel);
        if (icon) icon->setColor(label.light());
    }
}
