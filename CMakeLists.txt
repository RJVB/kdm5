project(KDM)
# set_package_properties appeared in cmake 2.8.6
# TODO: Remove when kdelibs >= 4.10 will be required
cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

# Used e.g. in KDE4WorkspaceConfig.cmake, Alex
set(KDM5_VERSION_MAJOR 4)
set(KDM5_VERSION_MINOR 99)
set(KDM5_VERSION_PATCH 90)
set(KDM5_VERSION ${KDM5_VERSION_MAJOR}.${KDM5_VERSION_MINOR}.${KDM5_VERSION_PATCH} )

find_package(ECM 0.0.8 REQUIRED NO_MODULE)

include(CMakePackageConfigHelpers)
include(FeatureSummary)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
include(ECMMarkNonGuiExecutable)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

include(ECMOptionalAddSubdirectory)
include(ECMInstallIcons)

set(CMAKE_CXX_STANDARD 11)

#search packages used by KDE
set(QT_MIN_VERSION "5.9.0")

# find_package(KDE4 4.9.4 REQUIRED)
# include(KDE4Defaults)

option(KDM_MULTIHEAD_SUPPORT "Xinerama support for multi-headed X displays" ON)

option(KDM_THEMEABLE "Build KDM with themeing support" OFF)

option(KDM5_RPCAUTH "Use Sun's secure RPC for Xauth cookies in KDM" OFF)
option(KDM5_KRB5AUTH "Use Kerberos5 for Xauth cookies in KDM" OFF)
option(KDM5_KERBEROS4 "Compile KDM with Kerberos v4 (sic!) support" OFF)
if (KDM5_KERBEROS4)
    option(KDM_AFS "Compile KDM with AFS support" OFF)
endif()

option(KDM5_XDMCP "Build KDM with XDMCP support" ON)
option(KDM5_XCONSOLE "Build KDM with built-in xconsole" OFF)


#TODO: this was coming from KDELibs4, we need a replacement
set(KDE_PLATFORM_PROFILE "Desktop")

find_package(Qt5Core REQUIRED NO_MODULE)
find_package(Qt5DBus REQUIRED NO_MODULE)
find_package(Qt5Widgets REQUIRED NO_MODULE)
find_package(Qt5Gui REQUIRED NO_MODULE)
find_package(Qt5X11Extras REQUIRED NO_MODULE)
find_package(Qt5UiTools REQUIRED NO_MODULE)
find_package(Qt5Svg REQUIRED NO_MODULE)

#include(ConfigureChecks.cmake)

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0)

# Load the frameworks we need
find_package(KF5 REQUIRED COMPONENTS
    Auth Config ConfigWidgets Crash DBusAddons DocTools 
    GuiAddons I18n IconThemes 
    KCMUtils KIO Parts Plasma Pty Wallet
    KDE4Support)

if (WITH_KDM_XCONSOLE)
    find_package(KF5Pty REQUIRED)
endif()

find_package(Q5ImageBlitz)
set_package_properties(Q5ImageBlitz PROPERTIES DESCRIPTION "An image effects library for Qt5"
                       URL "https://github.com/RJVB/q5imageblitz"
                       TYPE REQUIRED
                      )

find_package(XCB REQUIRED)
set_package_properties(XCB PROPERTIES DESCRIPTION "X protocol C-language Binding"
                     URL "http://xcb.freedesktop.org"
                     TYPE REQUIRED
                    )

find_package(X11_XCB)
set_package_properties(X11_XCB PROPERTIES DESCRIPTION "XCB X11 protocol client library"
                     TYPE REQUIRED
                    )

find_package(X11 REQUIRED)
set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
                     URL "http://www.x.org"
                     TYPE REQUIRED
                    )

add_feature_info("Automated testing of X clients" X11_XTest_FOUND
               "The X11 Testing Resource extension library is useful for automated testing of X clients")
add_feature_info("libXau" X11_Xau_FOUND "The X11 Authorization Protocol library may be used by KDM")
add_feature_info("LibXdmcp" X11_Xdmcp_FOUND "The X Display Manager Control Protocol library may be used by KDM")

find_package(Perl REQUIRED)

include(ConfigureChecks.cmake)
if(NOT WIN32)
  configure_file(config-unix.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-unix.h )
  configure_file(config-X11.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-X11.h )
#   if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
#     set(EXPORT_XCURSOR_PATH "XCURSOR_PATH=${CMAKE_INSTALL_PREFIX}/share/icons:$XCURSOR_PATH\":~/.icons:/usr/share/icons:/usr/share/pixmaps:/usr/X11R6/lib/X11/icons\"; export XCURSOR_PATH")
#   endif(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
#   configure_file(startkde.cmake ${CMAKE_CURRENT_BINARY_DIR}/startkde  @ONLY)
endif(NOT WIN32)
configure_file(config-workspace.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-workspace.h )

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS} -DHAVE_CONFIG_H=1)
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} ${KACTIVITIES_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/libs)

# libs
add_subdirectory(cmake)
add_subdirectory(libs)

# KDM doesn't actually have a hard dependency on neither Xau nor Xdmcp
# if(X11_Xau_FOUND AND X11_Xdmcp_FOUND)
  ecm_optional_add_subdirectory( kdm )
# endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

# make the libraries installed from kdebase/workspace available to other projects
# by creating and installing a KDE4WorkspaceConfig.cmake file, which will
# be searched and found by kdelibs/cmake/modules/FindKDE4Workspace.cmake. Alex

# now create the KDE4WorkspaceConfig.cmake file, which will be loaded by 
# kdelibs/cmake/modules/FindKDE4Workspace.cmake and which has to contain all information
# about the libraries installed from kdebase/workspace/libs/ anybody would like to have. Alex

# we need the absolute directories where stuff will be installed too
# but since the variables which contain the destinations can be relative
# or absolute paths, we need this macro to make them all absoulte, Alex
macro(MAKE_INSTALL_PATH_ABSOLUTE out in)
   if (IS_ABSOLUTE "${in}")    # IS_ABSOLUTE is new since cmake 2.4.8
      set(${out} "${in}")
   else (IS_ABSOLUTE "${in}")
      set(${out} "\${KDM5_INSTALL_DIR}/${in}")
   endif (IS_ABSOLUTE "${in}")
endmacro(MAKE_INSTALL_PATH_ABSOLUTE out in)

make_install_path_absolute(KDM5_LIB_DIR     ${LIB_INSTALL_DIR})
make_install_path_absolute(KDM5_LIBEXEC_DIR ${LIBEXEC_INSTALL_DIR})
make_install_path_absolute(KDM5_INCLUDE_DIR ${INCLUDE_INSTALL_DIR})
make_install_path_absolute(KDM5_BIN_DIR     ${BIN_INSTALL_DIR})
make_install_path_absolute(KDM5_SBIN_DIR    ${SBIN_INSTALL_DIR})
make_install_path_absolute(KDM5_DATA_DIR    ${DATA_INSTALL_DIR})
make_install_path_absolute(KDM5_HTML_DIR    ${HTML_INSTALL_DIR})
make_install_path_absolute(KDM5_CONFIG_DIR  ${CONFIG_INSTALL_DIR})
make_install_path_absolute(KDM5_ICON_DIR    ${ICON_INSTALL_DIR})
make_install_path_absolute(KDM5_KCFG_DIR    ${KCFG_INSTALL_DIR})
make_install_path_absolute(KDM5_LOCALE_DIR  ${LOCALE_INSTALL_DIR})
# make_install_path_absolute(KDM5_MIME_DIR    ${MIME_INSTALL_DIR})
make_install_path_absolute(KDM5_SOUND_DIR   ${SOUND_INSTALL_DIR})
make_install_path_absolute(KDM5_TEMPLATES_DIR    ${TEMPLATES_INSTALL_DIR})
make_install_path_absolute(KDM5_WALLPAPER_DIR    ${WALLPAPER_INSTALL_DIR})
make_install_path_absolute(KDM5_KCONF_UPDATE_DIR    ${KCONF_UPDATE_INSTALL_DIR})
make_install_path_absolute(KDM5_AUTOSTART_DIR    ${AUTOSTART_INSTALL_DIR})
make_install_path_absolute(KDM5_XDG_APPS_DIR        ${XDG_APPS_INSTALL_DIR})
make_install_path_absolute(KDM5_XDG_DIRECTORY_DIR   ${XDG_DIRECTORY_INSTALL_DIR})
make_install_path_absolute(KDM5_SYSCONF_DIR ${SYSCONF_INSTALL_DIR})
make_install_path_absolute(KDM5_MAN_DIR     ${MAN_INSTALL_DIR})
# make_install_path_absolute(KDM5_INFO_DIR    ${INFO_INSTALL_DIR})
make_install_path_absolute(KDM5_DBUS_INTERFACES_DIR ${DBUS_INTERFACES_INSTALL_DIR})
make_install_path_absolute(KDM5_DBUS_SERVICES_DIR   ${DBUS_SERVICES_INSTALL_DIR})
make_install_path_absolute(KDM5_SERVICES_DIR ${SERVICES_INSTALL_DIR})
make_install_path_absolute(KDM5_SERVICETYPES_DIR ${SERVICETYPES_INSTALL_DIR})

set(KDM5_TARGET_PREFIX KDE4Workspace__)

