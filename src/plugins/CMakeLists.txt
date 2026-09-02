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
