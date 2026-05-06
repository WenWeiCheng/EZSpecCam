#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QSettings>
#include <QHash>
#include <QVariant>
#include <QStandardPaths>
#include "CameraTypes.h"

/**
 * @file ConfigurationManager.h
 * @brief Merged Configuration Manager for Camera Settings Persistence
 *
 * Manages camera parameter configurations using QSettings with INI format.
 * This is a merged class combining ConfigSerializer and ParameterValidator
 * functionality for unified config management.
 *
 * Provides:
 * - INI-based persistence via QSettings
 * - Parameter validation logic
 * - Type-prefixed QVariant serialization
 * - Platform-specific storage paths via QStandardPaths
 *
 * @note This class is not thread-safe. Synchronize access when using
 * from multiple threads.
 *
 * @see ICameraDriver for parameter definitions
 */
class ConfigurationManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new Configuration Manager
     *
     * Initializes the manager.
     *
     * @param parent Optional parent QObject for ownership hierarchy
     */
    explicit ConfigurationManager(QObject *parent = nullptr);

    //==========================================================================
    // Configuration Persistence API
    //==========================================================================

    /**
     * @brief Save dynamic parameters to an INI file
     *
     * Saves a hash of dynamic parameters (exposure, gain, ROI, etc.) to an
     * INI file using the camera ID. This stores only the runtime-adjustable
     * parameters.
     *
     * @param cameraId Camera identifier (e.g., "mock-001")
     * @param parameters Hash of parameter name to QVariant value
     *
     * @note Uses type prefixes for QVariant round-trip (double:, int:, bool:, string:)
     * @see loadDynamicConfig() for loading parameters
     * @see getConfigPath() for the file path used
     */
    void saveDynamicConfig(const QString &cameraId, const QHash<QString, QVariant> &parameters);

    /**
     * @brief Load dynamic parameters from an INI file
     *
     * Loads dynamic parameters from the INI file associated with the camera ID.
     * Returns an empty hash if the file doesn't exist or is invalid.
     *
     * @param cameraId Camera identifier (e.g., "mock-001")
     * @return Hash of parameter name to QVariant value
     *
     * @note Type information is preserved from saveDynamicConfig()
     * @note Does NOT auto-apply loaded config - caller must process parameters
     * @see saveDynamicConfig() for saving parameters
     * @see getConfigPath() for the file path used
     */
    QHash<QString, QVariant> loadDynamicConfig(const QString &cameraId);

    /**
     * @brief Get the directory for storing camera configurations
     *
     * Returns the platform-specific application data location with a "configs"
     * subdirectory for storing per-camera configuration files.
     *
     * @return Path to config directory:
     *   - Windows: C:/Users/<username>/AppData/Local/EZSpecCam/configs/
     *   - Linux: ~/.local/share/EZSpecCam/configs/
     *   - macOS: ~/Library/Application Support/EZSpecCam/configs/
     *
     * @note Uses QStandardPaths::AppDataLocation for cross-platform compatibility
     * @note The directory is created automatically by saveDynamicConfig() if needed
     * @see getConfigPath() for camera-specific config file paths
     */
    static QString getConfigDirectory();

    /**
     * @brief Get the file path for a specific camera's configuration
     *
     * Constructs the full path to a camera configuration file based on the
     * camera identifier. The file is stored in the config directory.
     *
     * @param cameraId Camera identifier string (e.g., "mock-001", "camera-001")
     * @return Full path to camera config file (e.g., ".../configs/mock-001.ini")
     *
     * @note The cameraId is used directly as the filename (sanitized externally)
     * @note File extension is always .ini
     * @see getConfigDirectory() for the base directory path
     * @see saveDynamicConfig() for saving to this path
     * @see loadDynamicConfig() for loading from this path
     */
    static QString getConfigPath(const QString &cameraId);

    //==========================================================================
    // Parameter Validation API
    //==========================================================================

    /**
     * @brief Validate a value against a parameter constraint
     * @param value The value to validate
     * @param constraint The constraint to validate against
     * @param type The parameter type
     * @return true if valid, false otherwise
     */
    static bool validate(const QVariant &value,
                         const ParameterConstraint &constraint,
                         ParameterType type);

    /**
     * @brief Validate a value and return reason if invalid
     * @param value The value to validate
     * @param constraint The constraint to validate against
     * @param type The parameter type
     * @return Empty string if valid, otherwise description of why invalid
     */
    static QString validateReason(const QVariant &value,
                                  const ParameterConstraint &constraint,
                                  ParameterType type);

signals:
    /**
     * @brief Signal emitted when save operation fails
     * @param path File path where save failed
     * @param error Error message
     */
    void saveError(const QString &path, const QString &error);

    /**
     * @brief Signal emitted when load operation fails
     * @param path File path where load failed
     * @param error Error message
     */
    void loadError(const QString &path, const QString &error);

private:
    //==========================================================================
    // INI Serialization Helpers (merged from ConfigSerializer)
    //==========================================================================

    /**
     * @brief Save parameters to an INI file
     */
    static bool saveParameters(const QString &filePath,
                               const QString &cameraId,
                               const QVariantMap &parameters);

    /**
     * @brief Load parameters from an INI file
     */
    static bool loadParameters(const QString &filePath,
                              QString &cameraId,
                              QVariantMap &parameters);

    /**
     * @brief Save metadata to an INI file
     */
    static bool saveMetadata(const QString &filePath,
                             const QVariantMap &metadata);

    /**
     * @brief Load metadata from an INI file
     */
    static bool loadMetadata(const QString &filePath,
                            QVariantMap &metadata);

    /**
     * @brief Convert QVariant to type-prefixed string for INI storage
     */
    static QString variantToString(const QVariant &value);

    /**
     * @brief Convert type-prefixed string back to QVariant
     */
    static QVariant stringToVariant(const QString &valueStr);

    /**
     * @brief Ensure directory exists for given file path
     */
    static bool ensureDirectoryExists(const QString &filePath);
};

#endif // CONFIGURATIONMANAGER_H
