#ifndef EZSPECCAM_CORE_GLOBAL_H
#define EZSPECCAM_CORE_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(EZSPECCAM_CORE_LIBRARY)
#  define EZSPECCAM_CORE_EXPORT Q_DECL_EXPORT
#else
#  define EZSPECCAM_CORE_EXPORT Q_DECL_IMPORT
#endif

#define EZSPECCAM_CORE_NO_EXPORT Q_DECL_HIDDEN

// ===========================================
// Version Information
// ===========================================
#define EZSPECCAM_VERSION_MAJOR 1
#define EZSPECCAM_VERSION_MINOR 0
#define EZSPECCAM_VERSION_PATCH 0

// ===========================================
// Deprecation Macros
// ===========================================
#if defined(Q_CC_MSVC)
#  define EZSPECCAM_DEPRECATED __declspec(deprecated)
#else
#  define EZSPECCAM_DEPRECATED Q_DECL_DEPRECATED
#endif

#define EZSPECCAM_DEPRECATED_X(text) Q_DECL_DEPRECATED_X(text)

// ===========================================
// Export Macro Helper
// ===========================================
#define EZSPECCAM_DECLARE_PRIVATE(Class) \
    Class##Private *d_ptr; \
    Q_DECLARE_PRIVATE(Class)

#define EZSPECCAM_DECLARE_PUBLIC(Class) \
    Class *q_ptr; \
    Q_DECLARE_PUBLIC(Class)

#define EZSPECCAM_D(Class) Q_D(Class)
#define EZSPECCAM_Q(Class) Q_Q(Class)

#endif // EZSPECCAM_CORE_GLOBAL_H