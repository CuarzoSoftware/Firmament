#ifndef TOPBARMODEL_H
#define TOPBARMODEL_H

#include <Heaven/Bar/HNTopbar.h>
#include <AK/Nodes/AKContainer.h>

using namespace CZ;

/**
 * @brief Mirror of a Heaven HNTopbar (a client's menu bar).
 *
 * Owns one row container of menu titles per topbar surface (one per screen). When this topbar's
 * client becomes active, each TopbarSurface parents the corresponding row into its menu area.
 *
 * Ordering is kept in sync incrementally by reacting to Heaven's structural events
 * (onObjectInsertedBefore / onObjectParentChanged), not by rebuilding the whole row: insertMenu()
 * moves a single title node to its place; a detached menu removes its node.
 */
class TopbarModel final : public CZObject
{
public:

};

#endif // TOPBARMODEL_H
