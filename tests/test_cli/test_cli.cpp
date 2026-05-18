#include "test_cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <QtTest/QtTest>

#include "cli/CommandLineParser.h"
#include "cli/CaptureController.h"

class MockCameraDriver : public QObject
{
    Q_OBJECT
public:
    MockCameraDriver() : m_connected(false) {}

    bool connectToCamera(const QString &cameraId) {
        Q_UNUSED(cameraId);
        m_connected = true;
        return true;
    }

    void disconnectCamera() {
        m_connected = false;
    }

    bool isConnected() const { return m_connected; }

    QStringList enumerate() {
        return QStringList() << "mock-camera-1" << "mock-camera-2";
    }

    QString cameraId() const { return m_cameraId; }
    void setCameraId(const QString &id) { m_cameraId = id; }

    bool startCapture(int count = 0) {
        Q_UNUSED(count);
        if (!m_connected) return false;
        return true;
    }

    void stopCapture(int timeoutMs = 5000) {
        Q_UNUSED(timeoutMs);
    }

    QStringList parameterNames() const {
        return QStringList() << "exposure" << "gain";
    }

    QVariant parameterValue(const QString &name) const {
        if (name == "exposure") return m_exposure;
        if (name == "gain") return m_gain;
        return QVariant();
    }

    bool setParameter(const QString &name, const QVariant &value) {
        if (name == "exposure") m_exposure = value.toDouble();
        if (name == "gain") m_gain = value.toDouble();
        return true;
    }

    bool validateParameters() { return true; }
    bool commitParameters() { return true; }

    double exposure() const { return m_exposure; }
    double gain() const { return m_gain; }

signals:
    void frameReady(const QSharedPointer<QImage> &image, quint64 timestamp, int frameNumber, const QString &cameraId, const QVariantMap &parameters);
    void captureStarted(const QString &cameraId);
    void captureStopped(const QString &cameraId);
    void connectionChanged(bool connected, const QString &cameraId);
    void errorOccurred(const CameraError &error);

private:
    bool m_connected;
    QString m_cameraId;
    double m_exposure = 100.0;
    double m_gain = 1.0;
};

void TestCLI::init()
{
}

void TestCLI::cleanup()
{
}

void TestCLI::testCommandLineParserHelp()
{
    int argc = 2;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--help") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(parser.args().help);
    QVERIFY(!parser.args().listCameras);
    QVERIFY(parser.args().cameraId.isEmpty());
}

void TestCLI::testCommandLineParserListCameras()
{
    int argc = 2;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--list-cameras") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(!parser.args().help);
    QVERIFY(parser.args().listCameras);
}

void TestCLI::testCommandLineParserCameraId()
{
    int argc = 4;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--camera"), const_cast<char*>("mock-001") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(parser.args().cameraId == "mock-001");
}

void TestCLI::testCommandLineParserCapture()
{
    int argc = 4;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--camera"), const_cast<char*>("mock"), const_cast<char*>("--capture") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(parser.args().capture);
    QVERIFY(parser.args().cameraId == "mock");
}

void TestCLI::testCommandLineParserCount()
{
    int argc = 6;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--camera"), const_cast<char*>("mock"),
                     const_cast<char*>("--capture"), const_cast<char*>("--count"), const_cast<char*>("10") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(parser.args().capture);
    QVERIFY(parser.args().captureCount == 10);
}

void TestCLI::testCommandLineParserOutput()
{
    int argc = 6;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--camera"), const_cast<char*>("mock"),
                     const_cast<char*>("--capture"), const_cast<char*>("--output"), const_cast<char*>("./output") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(parser.args().outputDir == "./output");
}

void TestCLI::testCommandLineParserFormat()
{
    int argc = 6;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--camera"), const_cast<char*>("mock"),
                     const_cast<char*>("--capture"), const_cast<char*>("--format"), const_cast<char*>("jpg") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(parser.args().format == "jpg");
}

void TestCLI::testCommandLineParserExposure()
{
    int argc = 6;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--camera"), const_cast<char*>("mock"),
                     const_cast<char*>("--capture"), const_cast<char*>("--exposure"), const_cast<char*>("50.5") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(qAbs(parser.args().exposure - 50.5) < 0.001);
}

void TestCLI::testCommandLineParserGain()
{
    int argc = 6;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--camera"), const_cast<char*>("mock"),
                     const_cast<char*>("--capture"), const_cast<char*>("--gain"), const_cast<char*>("2.5") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(qAbs(parser.args().gain - 2.5) < 0.001);
}

void TestCLI::testCommandLineParserInvalidArgs()
{
    int argc = 3;
    char *argv[] = { const_cast<char*>("test"), const_cast<char*>("--camera"), const_cast<char*>("mock") };

    CommandLineParser parser;
    QVERIFY(parser.parse(argc, argv));
    QVERIFY(!parser.args().capture);
    QVERIFY(parser.args().cameraId == "mock");
}

void TestCLI::testCaptureControllerStateMachine()
{
    CommandLineArgs args;
    args.capture = true;
    args.cameraId = "mock-camera-1";
    args.captureCount = 1;
    args.exposure = 100.0;
    args.gain = 1.0;

    QVERIFY(args.isValid());
}

void TestCLI::testCaptureControllerInvalidArgs()
{
    CommandLineArgs args;
    args.capture = true;
    args.cameraId = "";
    QVERIFY(!args.isValid());
}

QTEST_MAIN(TestCLI)
#include "test_cli.moc"