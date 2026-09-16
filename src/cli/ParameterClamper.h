#pragma once

#include <QString>
#include <QVariantMap>

class ICameraDriver;

namespace cli
{

struct ApplyReport
{
    int clamped  = 0;  ///< Parameters silently corrected by clampValue()
    int skipped  = 0;  ///< Parameters rejected by clampValue()
    int setFailed = 0; ///< Parameters the driver->setParameter() refused
};

/**
 * @brief Apply a batch of (name, raw-value) pairs through clampValue and
 *        the driver's setParameter()/commitParameters() pipeline.
 *
 * Logs qWarning for each clamped/skipped/setFailed entry and qInfo for each
 * accepted value. Always calls commitParameters() once at the end; any names
 * it rejects are appended to a qWarning line.
 *
 * @param driver  Connected camera driver instance.
 * @param params  Map of parameter-name -> raw QVariant value.
 * @param indent  Prefix prepended to every log line (use "  " for sequence steps).
 * @return        Counts of how each entry was handled.
 */
ApplyReport applyClampedSet(ICameraDriver *driver,
                            const QVariantMap &params,
                            const QString &indent);

} // namespace cli