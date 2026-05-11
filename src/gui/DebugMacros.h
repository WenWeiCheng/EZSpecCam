#pragma once

#include <QLoggingCategory>
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(parameterCategory)
Q_DECLARE_LOGGING_CATEGORY(cameraCategory)
Q_DECLARE_LOGGING_CATEGORY(configCategory)
Q_DECLARE_LOGGING_CATEGORY(displayCategory)
Q_DECLARE_LOGGING_CATEGORY(captureCategory)

#define PARAM_PREFIX "[Parameter] "
#define CAMERA_PREFIX "[Camera] "
#define CONFIG_PREFIX "[Config] "
#define DISPLAY_PREFIX "[Display] "
#define CAPTURE_PREFIX "[Capture] "

#define CATEGORY_DEBUG(category, prefix) qCDebug(category) << "[" << prefix << __FILE__ << " +" << __LINE__ << "] "

#define PARAM_DEBUG CATEGORY_DEBUG(parameterCategory, PARAM_PREFIX)
#define CAMERA_DEBUG CATEGORY_DEBUG(cameraCategory, CAMERA_PREFIX)
#define CONFIG_DEBUG CATEGORY_DEBUG(configCategory, CONFIG_PREFIX)
#define DISPLAY_DEBUG CATEGORY_DEBUG(displayCategory, DISPLAY_PREFIX)
#define CAPTURE_DEBUG CATEGORY_DEBUG(captureCategory, CAPTURE_PREFIX)
