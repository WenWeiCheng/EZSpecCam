/**
 * @file commandlineparser.h
 * @brief Command-line argument parsing for EZSpecCam CLI application
 *
 * Supports headless camera control with options for camera selection,
 * capture modes, exposure, gain, and output configuration.
 */

#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include <QString>
#include <QStringList>

/**
 * @brief Command-line argument container with all parsed configuration values
 *
 * This struct holds all parsed command-line arguments with their default values.
 */
struct CommandLineArgs
{
    bool help = false;              ///< Display help message and exit
    bool listCameras = false;       ///< List available cameras and exit
    QString cameraId;               ///< Camera identifier to use
    bool capture = false;            ///< Start capture mode
    int captureCount = 0;           ///< Number of frames (0=continuous)
    QString outputDir;              ///< Output directory path
    QString format;                 ///< Image format: "tiff" or "jpg"
    double exposure = 100.0;        ///< Exposure time in milliseconds
    double gain = 1.0;              ///< Gain value

    bool isValid() const;

    static QString helpText();
};

/**
 * @brief Command-line argument parser for EZSpecCam CLI application
 */
class CommandLineParser
{
public:
    CommandLineParser();

    bool parse(int argc, char *argv[]);

    const CommandLineArgs &args() const;
    QString errorMessage() const;

private:
    CommandLineArgs m_args;
    QString m_errorMessage;
};

#endif // COMMANDLINEPARSER_H