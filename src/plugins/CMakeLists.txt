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

# hk16011's SDK is not installed by the vendor but ships a CMake package and
# builds on both platforms. The subdirectory self-skips with a WARNING +
# return() when HK16011_ROOT is unset, so it is safe to add unconditionally.
add_subdirectory(hk16011)
