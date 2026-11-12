#include <Core/Assets.h>
#include <Core/Firmament.h>
#include <Core/Log.h>
#include <XDG/XDGKit.h>
#include <Ream/RCore.h>
#include <Ream/RDevice.h>
#include <Ream/RSurface.h>
#include <Ream/RPass.h>
#include <drm_fourcc.h>
#include <cassert>

std::shared_ptr<RImage> Assets::loadIcon(const std::string &icon, Int32 size) noexcept
{
    std::shared_ptr<RImage> img;

    if (const auto *iconInfo { findIcon(icon, size, 1) })
    {
        if (iconInfo->extensions() & XDGIcon::SVG)
            img = loadFile(iconInfo->getPath(XDGIcon::SVG), { size, size }, icon);

        if (!img && (iconInfo->extensions() & XDGIcon::PNG))
            img = loadFile(iconInfo->getPath(XDGIcon::PNG), { size, size }, icon);
    }

    return img;
}

void Assets::init() noexcept
{
    auto ream { RCore::Get() };
    auto fmt { ream->mainDevice()->textureFormats().formats().find(DRM_FORMAT_ABGR8888) };
    assert(fmt != ream->mainDevice()->textureFormats().formats().end());
    safeFormat = *fmt;

    // Installed with Firmament (see meson.build).
    logo = loadFile(std::filesystem::path(CZ_FIRMAMENT_ASSETS_DIR) / "Cuarzo.png", { 0, 0 });

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

const XDGIcon *Assets::findIcon(const std::string &name, Int32 size, Int32 scale) noexcept
{
    auto *xdgkit { GetFirm()->xdgkit.get() };

    if (!xdgkit || name.empty())
        return nullptr;

    return xdgkit->iconThemeManager().findIcon(
        name, size, scale, XDGIcon::PNG | XDGIcon::SVG, { DEFAULT_ICON_THEME, FALLBACK_ICON_THEME, "" });
}

std::shared_ptr<RImage> Assets::loadFile(const std::filesystem::path &path, SkISize size, const std::string &iconName) noexcept
{
    auto img { RImage::LoadFile(path, safeFormat, size) };

    if (img)
    {
        std::erase_if(m_sources, [](const auto &pair){ return pair.second.image.expired(); });
        m_sources[img.get()] = { img, iconName, path };
    }

    return img;
}

std::shared_ptr<RImage> Assets::sized(const std::shared_ptr<RImage> &image, Int32 pixels, Int32 scale) noexcept
{
    if (!image)
        return {};

    auto src { m_sources.find(image.get()) };

    if (src == m_sources.end() || src->second.image.expired())
        return image;

    const Source &source { src->second };
    scale = std::max(1, scale);
    const std::tuple<std::string, Int32, Int32> key { source.iconName.empty() ? source.path.string() : "icon:" + source.iconName, pixels, scale };

    if (auto it { m_sized.find(key) }; it != m_sized.end())
        return it->second;

    // Its larger side at that size, keeping the proportions.
    const SkISize s { image->size() };
    const Int32 longest { std::max(1, std::max(s.width(), s.height())) };
    const SkISize size { std::max(1, s.width() * pixels / longest), std::max(1, s.height() * pixels / longest) };

    // An SVG is rendered again at that size (the same file: other sizes of a theme may be different
    // artwork, e.g. padded for panels). A raster themed icon: the theme's file for that size and
    // scale (only scaled, antialiased, if the theme doesn't have that size).
    std::filesystem::path path { source.path };

    if (!source.iconName.empty() && source.path.extension() != ".svg")
        if (const auto *icon { findIcon(source.iconName, std::max(1, (pixels + scale - 1) / scale), scale) })
            path = icon->getPath((icon->extensions() & XDGIcon::SVG) ? XDGIcon::SVG : XDGIcon::PNG);

    auto img { RImage::LoadFile(path, safeFormat, size) };

    if (!img)
        img = image;

    m_sized[key] = img;
    return img;
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
