# Camera driver plugins
add_subdirectory(mock)

if(WIN32)
    add_subdirectory(qhyccd)
    add_subdirectory(hamamatsu)
else()
    message(STATUS "Linux build: skipping qhyccd/hamamatsu (Windows-only SDKs)")
endif()

# picam self-skips with WARNING + return() when SDK is missing, so it is safe
# to add unconditionally across platforms.
add_subdirectory(picam)

# hk16011 ships a Linux-only C SDK that is not installed by the vendor, and it
# self-skips with WARNING + return() when HK16011_ROOT is unset — so it is
# safe to add here too, but it can never build on Windows.
if(NOT WIN32)
    add_subdirectory(hk16011)
else()
    message(STATUS "Windows build: skipping hk16011 (Linux-only SDK)")
endif()
