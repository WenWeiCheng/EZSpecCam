#pragma once

#include <QString>

class ICameraDriver;

namespace app
{

/// Poll a driver parameter until all readings in a sliding window are within
/// `tolerance` of `target`, or until `timeoutSec` elapses.
/// @return true if stable; false on timeout or read failure.
bool waitForStable(ICameraDriver *driver,
                   const QString &paramName,
                   double target,
                   double tolerance,
                   double windowSec,
                   double timeoutSec);

}
