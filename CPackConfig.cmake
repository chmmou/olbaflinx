include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "olbaflinx")
set(CPACK_PACKAGE_VENDOR "Alexander Saal")
set(CPACK_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OlbaFlinx is an multibank-capable online banking software for Linux based on the popular AqBanking library and the Qt 6 framework.

OlbaFlinx has the advantage that it is made for people who are just about to switch to Linux and or have switched and are looking for a simple financial software. The other advantage is that OlbaFlinx runs on any Linux Desktop Environment that supports the Qt 6 framework. This makes it possible for users to decide which desktop environment they want to use.

The idea to develop OlbaFlinx came from the fact that I was looking for a simple financial software for Linux that had the simplicity of Banking4 (Windows / Mac). Unfortunately, none of the existing graphical financial software could convince me.")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "TGZ;TZST")
set(CPACK_SOURCE_GENERATOR "TXZ;TBZ2;TGZ")
set(CPACK_SOURCE_IGNORE_FILES
        \\.git/
        \\.qt/
        \\.github/
        cbuild/
        ".*~$"
)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Alexander Saal")
set(CPACK_PACKAGE_CONTACT "developer@olbaflinx.chm-projects.de")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)
