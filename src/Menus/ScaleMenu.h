#ifndef SCALEMENU_H
#define SCALEMENU_H

#include <Menus/MenuBuilder.h>
#include <vector>

/**
 * @brief The size menu at the right end of the bar: ×1 to ×2 (see TOPBAR_SCALES).
 *
 * Scales the bar and its menus on every screen (see Firmament::setUIScale()).
 */
class ScaleMenu final : public MenuBuilder
{
public:
    ScaleMenu() noexcept;

    MenuModel *menu() const noexcept { return m_menu; }

    /// Checks the current scale's item.
    void update() noexcept;

    /// The label of a scale, e.g. "×1.75".
    static std::string Label(float scale) noexcept;

private:
    MenuModel *m_menu {};
    std::vector<std::pair<Toggle*, float>> m_toggles;
};

#endif // SCALEMENU_H
