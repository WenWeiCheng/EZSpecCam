#include "WaitStabilizer.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QLatin1Char>
#include <QTextStream>
#include <QTimer>
#include <QVariant>
#include <QVector>

#include "ICameraDriver.h"

namespace app
{

namespace
{
struct Sample { qint64 ms; double value; };
}

bool waitForStable(ICameraDriver *driver,
                   const QString &paramName,
                   double target,
                   double tolerance,
                   double windowSec,
                   double timeoutSec)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    QVector<Sample> samples;
    QEventLoop loop;
    QTextStream statusOut(stdout);
    int exitCode = -1;

    QTimer pollTimer;
    pollTimer.setInterval(500);

    QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
        QVariant val = driver->parameterValue(paramName);
        if (!val.isValid()) {
            statusOut << '\n'; statusOut.flush();
            exitCode = -1; loop.quit(); return;
        }
        double current = val.toDouble();
        qint64 elapsed = totalTimer.elapsed();
        samples.append({ elapsed, current });

        while (!samples.isEmpty() && (elapsed - samples.first().ms) > windowSec * 1000)
            samples.removeFirst();

        QString status = QStringLiteral("  %1 = %2 (target: %3, delta=%4, elapsed: %5s)")
            .arg(paramName, -28)
            .arg(current, 8, 'f', 2)
            .arg(target, 8, 'f', 2)
            .arg(qAbs(current - target), 0, 'f', 3)
            .arg(elapsed / 1000.0, 0, 'f', 1);
        status = status.leftJustified(79, QLatin1Char(' '));
        statusOut << '\r' << status; statusOut.flush();

        if (samples.size() >= 3) {
            bool stable = true;
            for (const auto &s : samples) {
                if (qAbs(s.value - target) > tolerance) { stable = false; break; }
            }
            if (stable) {
                double windowMs = windowSec * 1000;
                double actualWindow = elapsed - samples.first().ms;
                if (actualWindow >= windowMs * 0.9) {
                    statusOut << '\n'; statusOut.flush();
                    exitCode = 0; loop.quit(); return;
                }
            }
        }

        if (elapsed > timeoutSec * 1000) {
            statusOut << '\n'; statusOut.flush();
            exitCode = -1; loop.quit();
        }
    });

    pollTimer.start();
    loop.exec();
    pollTimer.stop();
    return exitCode == 0;
}

}
