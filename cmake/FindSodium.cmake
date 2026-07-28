# FindSodium.cmake
# Finds the libsodium library
#
# This will define the following variables:
#   Sodium_FOUND        - True if the system has libsodium
#   Sodium_INCLUDE_DIRS - Include directories for libsodium
#   Sodium_LIBRARIES    - Libraries to link against
#
# and the following imported targets:
#   Sodium::Sodium - The libsodium library

find_path(Sodium_INCLUDE_DIR
    NAMES sodium.h
    PATHS
        /usr/local/include
        /usr/include
)

find_library(Sodium_LIBRARY
    NAMES sodium
    PATHS
        /usr/local/lib
        /usr/lib
        /opt/homebrew/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sodium
    REQUIRED_VARS Sodium_LIBRARY Sodium_INCLUDE_DIR
)

if(Sodium_FOUND AND NOT TARGET Sodium::Sodium)
    add_library(Sodium::Sodium UNKNOWN IMPORTED)
    set_target_properties(Sodium::Sodium PROPERTIES
        IMPORTED_LOCATION "${Sodium_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Sodium_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(Sodium_INCLUDE_DIR Sodium_LIBRARY)
