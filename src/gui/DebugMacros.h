#pragma once

#include <QLoggingCategory>
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(parameterCategory)
Q_DECLARE_LOGGING_CATEGORY(cameraCategory)
Q_DECLARE_LOGGING_CATEGORY(configCategory)
Q_DECLARE_LOGGING_CATEGORY(displayCategory)
Q_DECLARE_LOGGING_CATEGORY(captureCategory)

#define CATEGORY_DEBUG(category) qCDebug(category) << "[" << __FILE__ << " +" << __LINE__ << "] "

#define PARAM_DEBUG CATEGORY_DEBUG(parameterCategory)
#define CAMERA_DEBUG CATEGORY_DEBUG(cameraCategory)
#define CONFIG_DEBUG CATEGORY_DEBUG(configCategory)
#define DISPLAY_DEBUG CATEGORY_DEBUG(displayCategory)
#define CAPTURE_DEBUG CATEGORY_DEBUG(captureCategory)
