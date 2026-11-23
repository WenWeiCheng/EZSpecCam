#include "FileNaming.h"

namespace FileNaming
{

QString frameFileName(const QString &prefix, const QString &suffix,
                      const QString &imageFormat, const QDateTime &timestamp)
{
    const QString prefixPart = prefix.isEmpty() ? QString() : prefix + QLatin1Char('_');
    const QString suffixPart = suffix.isEmpty() ? QString() : QLatin1Char('_') + suffix;
    const QString extension = imageFormat.toLower();
    return QStringLiteral("%1img_%2%3.%4")
        .arg(prefixPart, timestamp.toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")),
             suffixPart, extension);
}

QString referenceStem(Calibration::ReferenceKind kind)
{
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        return QStringLiteral("dark");
    case Calibration::ReferenceKind::Bias:
        return QStringLiteral("bias");
    case Calibration::ReferenceKind::LongDark:
        return QStringLiteral("long_dark");
    case Calibration::ReferenceKind::Flat:
        return QStringLiteral("flat");
    case Calibration::ReferenceKind::VffEntry:
        return QStringLiteral("vff_flat");
    }
    return QStringLiteral("reference");
}

QString referenceFileName(const QString &stem,
                          const Calibration::ReferenceFrame &reference,
                          const QDateTime &timestamp)
{
    QString name = stem;
    if (reference.exposure > 0.0) {
        QString exposure = QString::number(reference.exposure, 'g', 6);
        exposure.replace(QLatin1Char('.'), QLatin1Char('p'));
        name += QStringLiteral("_%1s").arg(exposure);
    }
    if (reference.frameCount > 0) {
        name += QStringLiteral("_avg%1").arg(reference.frameCount);
    }
    name += QLatin1Char('_');
    name += timestamp.toString(QStringLiteral("yyyyMMdd_hhmmss"));
    return name;
}

} // namespace FileNaming
