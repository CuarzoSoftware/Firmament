#include <Menus/ScaleMenu.h>
#include <Models/MenuModel.h>
#include <Nodes/Toggle.h>
#include <Core/Firmament.h>
#include <cmath>
#include <format>

ScaleMenu::ScaleMenu() noexcept
{
    m_menu = newMenu();
    addDivider(m_menu, "Bar Size");

    for (const float scale : TOPBAR_SCALES)
    {
        Toggle *toggle { addToggle(m_menu, Label(scale)) };
        toggle->onClicked = [scale]{ GetFirm()->setUIScale(scale); };
        m_toggles.emplace_back(toggle, scale);
    }
}

void ScaleMenu::update() noexcept
{
    for (auto &[toggle, scale] : m_toggles)
        toggle->setChecked(std::fabs(scale - GetFirm()->uiScale()) < 0.01f);
}

std::string ScaleMenu::Label(float scale) noexcept
{
    // ×1, ×1.5, ×1.75, ×2
    std::string number { std::format("{:.2f}", scale) };

    while (number.back() == '0')
        number.pop_back();

    if (number.back() == '.')
        number.pop_back();

    return "\u00D7" + number;
}
