#ifndef MENUITEM_H
#define MENUITEM_H

#include <AK/Nodes/AKContainer.h>
#include <AK/Nodes/AKText.h>
#include <functional>
#include <string>

using namespace CZ;

/**
 * @brief Base class for every row shown inside a MenuSurface (action, toggle, divider, submenu).
 *
 * On pointer enter it becomes the surface's active item (hover + selection outline). The virtual
 * setters below let the Heaven layer update the item generically without knowing its concrete type;
 * the default implementations are no-ops so each subclass only overrides what it supports.
 */
class MenuItem : public AKContainer
{
public:
    MenuItem(AKNode *parent = nullptr) noexcept;

    virtual void setHover(bool hover) noexcept { CZ_UNUSED(hover) }
    virtual void siblingsChanged() noexcept {}

    virtual void setText(const std::string &text) noexcept { CZ_UNUSED(text) }
    virtual void setIconName(const std::string &iconName) noexcept { CZ_UNUSED(iconName) }
    virtual void setShortcut(const std::string &shortcut) noexcept { CZ_UNUSED(shortcut) }
    virtual void setEnabled(bool enabled) noexcept { CZ_UNUSED(enabled) }
    virtual void setChecked(bool checked) noexcept { CZ_UNUSED(checked) }

    /// Whether the item can be selected/activated. Disabled items are not hovered and do not get the
    /// selection outline. Defaults to true; interactive subclasses override it.
    virtual bool enabled() const noexcept { return true; }

    /// Invoked when the item is activated (pointer released over it). Set by the owner (Firmament)
    /// to forward the click to the backing Heaven object; left unset for non-interactive items.
    std::function<void()> onClicked;

    void pointerEnterEvent(const CZPointerEnterEvent &e) override;
    void pointerButtonEvent(const CZPointerButtonEvent &e) override;
};

#endif // MENUITEM_H
