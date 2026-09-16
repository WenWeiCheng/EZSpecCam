#include "ParameterClamper.h"
#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"

#include <QDebug>

namespace cli
{

ApplyReport applyClampedSet(ICameraDriver *driver,
                            const QVariantMap &params,
                            const QString &indent)
{
    ApplyReport rep;
    if (!driver) return rep;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &raw  = it.value();
        ParameterDefinition def = driver->parameter(name);
        ClampResult r = clampValue(raw, def);
        if (r.changed) {
            qWarning().noquote() << indent + name + ": " + r.reason;
            ++rep.clamped;
        }
        if (!r.value.isValid()) {
            qWarning().noquote() << indent + "Skipped" << name;
            ++rep.skipped;
            continue;
        }
        qInfo().noquote() << indent + "set " + name + " = " + r.value.toString();
        if (!driver->setParameter(name, r.value)) {
            qWarning().noquote() << indent + "Failed to set " + name;
            ++rep.setFailed;
        }
    }
    QStringList commitFailed;
    driver->commitParameters(&commitFailed);
    if (!commitFailed.isEmpty())
        qWarning().noquote() << indent + "commitParameters rejected: "
                             + commitFailed.join(", ");
    return rep;
}

} // namespace cli