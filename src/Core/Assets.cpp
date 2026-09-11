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
    auto *xdgkit { GetFirm()->xdgkit.get() };
    std::shared_ptr<RImage> img;

    const auto *iconInfo { xdgkit->iconThemeManager().findIcon(
        icon, size, 1, XDGIcon::PNG | XDGIcon::SVG, { DEFAULT_ICON_THEME, "" }) };

    if (iconInfo)
    {
        if (iconInfo->extensions() & XDGIcon::SVG)
            img = RImage::LoadFile(iconInfo->getPath(XDGIcon::SVG), safeFormat, { size, size });

        if (!img && (iconInfo->extensions() & XDGIcon::PNG))
            img = RImage::LoadFile(iconInfo->getPath(XDGIcon::PNG), safeFormat, { size, size });
    }

    return img;
}

void Assets::init() noexcept
{
    auto ream { RCore::Get() };
    auto fmt { ream->mainDevice()->textureFormats().formats().find(DRM_FORMAT_ABGR8888) };
    assert(fmt != ream->mainDevice()->textureFormats().formats().end());
    safeFormat = *fmt;

    logo = RImage::LoadFile("/home/eduardo/Cuarzo.png", safeFormat);

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
