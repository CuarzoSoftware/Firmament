#include <Core/Assets.h>
#include <Core/Firmament.h>
#include <Core/Log.h>
#include <AssetBroker/ABCore.h>
#include <Ream/RSurface.h>
#include <Ream/RPass.h>
#include <cassert>

std::shared_ptr<RImage> Assets::loadIcon(const std::string &icon, Int32 size) noexcept
{
    if (!m_ab || icon.empty())
        return {};

    ABIconKey key;
    key.names = { icon };
    key.size = size;
    key.themes = { DEFAULT_ICON_THEME, FALLBACK_ICON_THEME, "" };

    auto img { m_ab->icon(key, { size, size }) };

    if (img)
        remember(img, { img, key, {} });

    return img;
}

void Assets::init() noexcept
{
    // Icons and images: through the cz-asset-broker service if it's running (shared with other
    // apps as DMA-bufs, looked up and rasterized once), otherwise loaded here. Local lookups reuse
    // Firmament's XDGKit.
    m_ab = ABCore::GetOrMake();

    if (m_ab)
    {
        m_ab->setXDGKit(GetFirm()->xdgkit);
        Log(CZInfo, CZLN, "Icons from {}", m_ab->remote() ? "the asset broker service" : "this process");

        // Installed with Firmament (see meson.build).
        const auto logoPath { std::filesystem::path(CZ_FIRMAMENT_ASSETS_DIR) / "Cuarzo.png" };
        logo = m_ab->image(logoPath);

        if (logo)
            remember(logo, { logo, {}, logoPath });
    }
    else
        Log(CZError, CZLN, "Failed to create the ABCore: icons won't load");

    if (!logo)
        Log(CZError, CZLN, "Failed to load the logo from {} (is Firmament installed?)", CZ_FIRMAMENT_ASSETS_DIR);

    // A 5x3 nine-slice-ish patch with a 1px horizontal line, used by menu dividers.
    auto surface { RSurface::Make(SkISize(5, 3), 1, true) };
    assert(surface);
    auto pass { surface->beginPass(RPassCap::RPassCap_SkCanvas) };
    assert(pass);
    auto *c { pass->getCanvas() };
    c->clear(SK_ColorTRANSPARENT);
    SkPaint p;
    p.setColor(SK_ColorBLACK);
    p.setAntiAlias(true);
    p.setStroke(true);
    p.setStrokeWidth(1);
    c->drawLine(SkPoint(0, 1), SkPoint(5, 1), p);
    pass.reset();
    divider3Patch = surface->image();

    check = loadIcon("theme-check-symbolic", 48);
    if (check) Log(CZInfo, "Found theme-check-symbolic icon for toggle");
    else       Log(CZError, "Failed to find theme-check-symbolic icon for toggle");

    caret = loadIcon("arrow-right", 48);
    if (caret) Log(CZInfo, "Found arrow-right icon for submenu");
    else       Log(CZError, "Failed to find arrow-right icon for submenu");
}

/* ---------------------------------- Scale specific images ---------------------------------- */

void Assets::remember(const std::shared_ptr<RImage> &image, Source source) noexcept
{
    std::erase_if(m_sources, [](const auto &pair){ return pair.second.image.expired(); });
    m_sources[image.get()] = std::move(source);
}

std::shared_ptr<RImage> Assets::sized(const std::shared_ptr<RImage> &image, Int32 pixels, Int32 scale) noexcept
{
    CZ_UNUSED(scale) // pixels are already physical: the same artwork for every scale

    if (!image || !m_ab)
        return image;

    auto src { m_sources.find(image.get()) };

    if (src == m_sources.end() || src->second.image.expired())
        return image;

    // Its larger side at that size, keeping the proportions. The same lookup (so an SVG is rendered
    // again from the same file: other sizes of a theme may be different artwork, e.g. padded for
    // panels). Shared and cached by the ABCore (and the service).
    const SkISize box { std::max(1, pixels), std::max(1, pixels) };
    const Source &source { src->second };
    auto img { source.path.empty() ? m_ab->icon(source.key, box) : m_ab->image(source.path, box) };
    return img ? img : image;
}

AKImageFrame::ImageProvider Assets::provider(std::shared_ptr<RImage> image) noexcept
{
    if (!image)
        return nullptr;

    return [this, image](Int32 pixels, Int32 scale){ return sized(image, pixels, scale); };
}

void Assets::show(AKImageFrame &frame, std::shared_ptr<RImage> image) noexcept
{
    if (image)
        frame.setImageProvider(provider(image));
    else
        frame.setImage(nullptr);
}
