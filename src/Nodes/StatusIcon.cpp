#include <Nodes/StatusIcon.h>
#include <Core/Firmament.h>
#include <Core/Assets.h>
#include <AK/AKTheme.h>
#include <unordered_map>

// Icons are shared by every bar (one per screen) and change often (e.g. the Wi-Fi signal).
static std::shared_ptr<RImage> Icon(const std::string &name) noexcept
{
    static std::unordered_map<std::string, std::shared_ptr<RImage>> cache;

    if (auto it { cache.find(name) }; it != cache.end())
        return it->second;

    // Loaded at the largest bar scale (2x for HiDPI), like the other bar icons.
    auto img { GetFirm()->assets.loadIcon(name, Int32(TOPBAR_ICON_SIZE * 2 * TOPBAR_MAX_SCALE)) };
    cache[name] = img;
    return img;
}

StatusIcon::StatusIcon(AKNode *parent) noexcept : AKImageFrame(parent)
{
    SkRegion empty;
    setInputRegion(&empty);
    setVisible(false);
    setAlignment(CZAlignCenter);
    setSizeMode(SizeMode::Contain);

    // Symbolic: tinted like the text (adapts to the color scheme).
    enableReplaceImageColor(true);
    const auto tint = [this]{ setColor(AKTheme::Text.resolve(colorScheme())); };
    tint();
    onColorSchemeChanged.subscribe(this, [tint](CZColorScheme){ tint(); });

    setUIScale(1.f);
}

void StatusIcon::setIconName(const std::string &name) noexcept
{
    if (m_name == name)
        return;

    m_name = name;
    auto img { name.empty() ? nullptr : Icon(name) };
    GetFirm()->assets.show(*this, img); // Sized for each screen's scale
    setVisible(img != nullptr);
}

void StatusIcon::setUIScale(float scale) noexcept
{
    layout().setWidth((TOPBAR_ICON_SIZE + 2) * scale);
    layout().setHeight((TOPBAR_ICON_SIZE + 2) * scale);
    layout().setMargin(YGEdgeHorizontal, TOPBAR_ITEM_PADDING * 0.5f * scale);
}
