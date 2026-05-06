#ifndef TEST_CLI_H
#define TEST_CLI_H

#include <QObject>
#include <QString>
#include <QStringList>

class CommandLineArgs;

class TestCLI : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testCommandLineParserHelp();
    void testCommandLineParserListCameras();
    void testCommandLineParserCameraId();
    void testCommandLineParserCapture();
    void testCommandLineParserCount();
    void testCommandLineParserOutput();
    void testCommandLineParserFormat();
    void testCommandLineParserExposure();
    void testCommandLineParserGain();
    void testCommandLineParserInvalidArgs();

    void testCaptureControllerStateMachine();
    void testCaptureControllerInvalidArgs();
};

#endif