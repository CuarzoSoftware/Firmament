#ifndef CZ_FIRMLOG_H
#define CZ_FIRMLOG_H

#include <CZ/Core/CZLogger.h>

#define Log FirmLogger()

const CZ::CZLogger &FirmLogger() noexcept;

#endif // CZ_FIRMLOG_H
