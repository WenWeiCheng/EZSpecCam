#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QSharedPointer>
#include <QTemporaryDir>

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "plugins/mock/MockCameraDriver.h"
#include "cli/HeadlessController.h"

class TestHeadlessController : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_driver = new MockCameraDriver();
        QVERIFY(m_driver->connectToCamera("mock-001"));
        m_driver->setParameter("exposure", 1.0);
        QVERIFY(m_driver->commitParameters());
        QVERIFY(m_outputDir.isValid());
    }

    void cleanup()
    {
        if (m_driver) {
            if (m_driver->isConnected()) {
                m_driver->disconnectCamera();
            }
            delete m_driver;
            m_driver = nullptr;
        }
    }

    /// Regression test: cli::captureFrames() must accept and capture exactly
    /// the requested number of frames, with no spurious "stopped
    /// unexpectedly" warnings triggered by stale connections from prior calls.
    void test_two_consecutive_captures_both_succeed()
    {
        const int framesPerCall = 3;
        const QString outDir = m_outputDir.path();

        int rc1 = cli::captureFrames(m_driver, framesPerCall,
                                     outDir, QStringLiteral("tiff"),
                                     QStringLiteral("call1_"),
                                     QString());
        QCOMPARE(rc1, framesPerCall);

        int rc2 = cli::captureFrames(m_driver, framesPerCall,
                                     outDir, QStringLiteral("tiff"),
                                     QStringLiteral("call2_"),
                                     QString());
        QCOMPARE(rc2, framesPerCall);
    }

    /// Three consecutive captureFrames() calls (the runSequence() scenario)
    /// must all complete with their requested frame count. Without the fix,
    /// stale lambdas from earlier calls accumulate on the driver and can
    /// either crash on dangling references or short-circuit later loops.
    void test_run_sequence_three_capture_steps()
    {
        const int framesPerStep = 2;
        const QString outDir = m_outputDir.path();

        int rc1 = cli::captureFrames(m_driver, framesPerStep,
                                     outDir, QStringLiteral("tiff"),
                                     QStringLiteral("seq1_"), QString());
        int rc2 = cli::captureFrames(m_driver, framesPerStep,
                                     outDir, QStringLiteral("tiff"),
                                     QStringLiteral("seq2_"), QString());
        int rc3 = cli::captureFrames(m_driver, framesPerStep,
                                     outDir, QStringLiteral("tiff"),
                                     QStringLiteral("seq3_"), QString());

        QCOMPARE(rc1, framesPerStep);
        QCOMPARE(rc2, framesPerStep);
        QCOMPARE(rc3, framesPerStep);
    }

    /// Direct test of the connection-lifecycle contract that the fix enforces.
    /// We replicate the connection pattern from captureFrames() twice in a
    /// row: the first call uses the *fixed* pattern (&loop context) and the
    /// second uses the *buggy* pattern (no context). After each block ends,
    /// we manually emit captureStopped and observe whether a stale lambda
    /// still fires.
    ///
    /// The buggy pattern (no context receiver) leaves the lambda connected to
    /// the driver for the rest of the process's lifetime; the fixed pattern
    /// (QObject as context) auto-disconnects when the QObject is destroyed.
    /// We detect this with a value-captured counter that survives the
    /// dangling-pointer UB of the buggy pattern.
    void test_no_stale_lambda_after_qobject_context_destroyed()
    {
        int buggyCounter = 0;
        int fixedCounter = 0;

        // --- Fixed pattern: &loop (a stack QObject) as context receiver ---
        {
            QObject loopContext;
            QObject::connect(m_driver, &ICameraDriver::captureStopped,
                             &loopContext,
                             [&fixedCounter](const QString &) { ++fixedCounter; });
            // loopContext goes out of scope here → all its connections torn down.
        }

        // --- Buggy pattern: no context receiver → lambda leaks ---
        {
            QObject::connect(m_driver, &ICameraDriver::captureStopped,
                             [&buggyCounter](const QString &) { ++buggyCounter; });
            // No context, no auto-cleanup. The lambda is still connected to
            // m_driver and uses a stack-allocated capture that survives only
            // because buggyCounter lives in the outer scope.
        }

        emit m_driver->captureStopped(m_driver->cameraId());

        QCOMPARE(fixedCounter, 0);
        QCOMPARE(buggyCounter, 1);
    }

private:
    MockCameraDriver *m_driver = nullptr;
    QTemporaryDir m_outputDir;
};

QTEST_MAIN(TestHeadlessController)
#include "test_headless_controller.moc"