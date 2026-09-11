#include <Core/Log.h>

using namespace CZ;

const CZ::CZLogger &FirmLogger() noexcept
{
    static CZLogger logger { "Firmament", "CZ_FIRMAMENT_LOG_LEVEL" };
    return logger;
}
