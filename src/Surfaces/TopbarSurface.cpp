#include <Marco/MScreen.h>
#include <Surfaces/TopbarSurface.h>
#include <Models/TopbarModel.h>
#include <Nodes/TopbarItem.h>
#include <Core/Firmament.h>

TopbarSurface::TopbarSurface(MScreen &screen) noexcept
{
    screen.userData = this;
    logo.setIcon(GetFirm()->logo);
    setScreen(&screen);

    vibrancy.layout().setWidthPercent(100.f);
    vibrancy.layout().setHeightPercent(100.f);
    vibrancy.layout().setFlexDirection(YGFlexDirectionRow);
    vibrancy.layout().setPadding(YGEdgeHorizontal, 12.f);
    vibrancy.layout().setAlignItems(YGAlignCenter);

    layout().setHeight(TOPBAR_HEIGHT);
    layout().setMaxHeight(TOPBAR_HEIGHT);
    layout().setMinHeight(TOPBAR_HEIGHT);

    // Fixed application title (bold), left.
    appTitle.setText("Desk");
    appTitle.title->setFontStyle(SkFontStyle::Bold());

    m1.setText("Archivo");
    m2.setText("Edición");
    m3.setText("Visualización");
    m4.setText("Ir");
    m5.setText("Ventana");
    m6.setText("Ayuda");

    // Menu area, right of the title.
    menusArea.layout().setFlexDirection(YGFlexDirectionRow);
    menusArea.layout().setAlignItems(YGAlignCenter);
    menusArea.layout().setHeightPercent(100.f);

    outline.layout().setPositionType(YGPositionTypeAbsolute);
    outline.layout().setPosition(YGEdgeTop, 0.f);
    outline.layout().setHeightPercent(100.f);
    outline.setVisible(false);

    setColor(CZAdaptiveColor(0));
    setExclusiveZone(TOPBAR_HEIGHT);
    setExclusiveEdge(CZEdgeTop);
    setLayer(Layer::Overlay);
    setAnchor(CZEdgeLeft | CZEdgeTop | CZEdgeRight);
    requestAvailableWidth();
    setMapped(true);
}

void TopbarSurface::setActiveItem(TopbarItem *item) noexcept
{
    if (!item)
    {
        outline.setVisible(false);
        return;
    }

    outline.layout().setPosition(YGEdgeLeft, item->worldRect().x() - TOPBAR_OUTLINE_PADDING);
    outline.layout().setWidth(item->worldRect().width() + 2 * TOPBAR_OUTLINE_PADDING);
    outline.setVisible(true);
}

void TopbarSurface::pointerButtonEvent(const CZPointerButtonEvent &e)
{
    MLayerSurface::pointerButtonEvent(e);

    if (e.button == BTN_LEFT && scene().nodeAt(AKApp::Get()->pointer().pos()) == &vibrancy)
        GetFirm()->setActiveTopbarItem(nullptr);
}
