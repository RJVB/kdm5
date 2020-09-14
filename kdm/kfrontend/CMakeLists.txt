include(MacroAddFileDependencies)

if (KDM_THEMEABLE)
    add_subdirectory( themes )
endif()
add_subdirectory( pics )
add_subdirectory( sessions )

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../kcm/background
	${KDM_SOURCE_DIR}/libs/kdm
	${QIMAGEBLITZ_INCLUDES}
)

if (KDM_THEMEABLE)
    set(kdmthemer_SRCS
        themer/kdmthemer.cpp
        themer/kdmthemer.h
        themer/kdmitem.cpp
        themer/kdmitem.h
        themer/kdmpixmap.cpp
        themer/kdmpixmap.h
        themer/kdmrect.cpp
        themer/kdmrect.h
        themer/kdmlist.cpp
        themer/kdmlist.h
        themer/kdmlabel.cpp
        themer/kdmlabel.h
        themer/kdmbutton.cpp
        themer/kdmbutton.h
        themer/kdmlayout.cpp
        themer/kdmlayout.h
        themer/parse.cpp
        themer/parse.h
    )
endif()
set(kdm_greet_SRCS
	kdm_greet.c
	kdm_greet.h
	kdmconfig.cpp
	kdmconfig.h
	kdmclock.cpp
	kdmclock.h
	kfdialog.cpp
	kfdialog.h
	kgdialog.cpp
	kgdialog.h
	kgverify.cpp
	kgverify.h
	kdmshutdown.cpp
	kdmshutdown.h
	kgreeter.cpp
	kgreeter.h
	kgapp.cpp
	kgapp.h
	utils.cpp
	utils.h
	${kdmthemer_SRCS}
)

if (XDMCP)
	set(kdm_greet_SRCS ${kdm_greet_SRCS}
		kchooser.cpp
		kchooser.h
	)
endif (XDMCP)

if (WITH_KDM_XCONSOLE)
	set(kdm_greet_SRCS ${kdm_greet_SRCS}
		kconsole.cpp
		kconsole.h
	)
endif (WITH_KDM_XCONSOLE)

macro_add_file_dependencies(kdmconfig.h ${confci})
add_executable(kdm_greet ${kdm_greet_SRCS})
ecm_mark_nongui_executable(kdm_greet)
# TODO: figure out why XCB::XCB doesn't get defined so we can NOT link to ALL XCB libraries
target_link_libraries(kdm_greet
    KF5::CoreAddons KF5::ConfigCore KF5::I18n
    KF5::WidgetsAddons KF5::KDELibs4Support
    Qt5::Core Qt5::Widgets Qt5::Network Qt5::Svg Qt5::X11Extras
    ${QT_QTXML_LIBRARY}
    ${X11_X11_LIB} ${XCB_LIBRARIES} ${POSIX4_LIBRARIES})
if (X11_XTest_FOUND)
  target_link_libraries(kdm_greet ${X11_XTest_LIB})
endif (X11_XTest_FOUND)
if (WITH_KDM_XCONSOLE)
  target_link_libraries(kdm_greet KF5::Pty)
endif (WITH_KDM_XCONSOLE)

install(TARGETS kdm_greet DESTINATION ${LIBEXEC_INSTALL_DIR})

set(kdm_config_SRCS kdm_config.c)
macro_add_file_dependencies(kdm_config.c ${confci})
add_executable(kdm5_config ${kdm_config_SRCS})
ecm_mark_nongui_executable(kdm5_config)
target_compile_options(kdm5_config PRIVATE -U_REENTRANT)
target_link_libraries(kdm5_config ${SOCKET_LIBRARIES} ${RESOLV_LIBRARIES} ${POSIX4_LIBRARIES})
add_dependencies(kdm5_config ConfigCi)
install(TARGETS kdm5_config DESTINATION ${LIBEXEC_INSTALL_DIR})

set(krootimage_SRCS krootimage.cpp krootimage.h ${backgroundlib_SRCS})
add_executable(krootimage ${krootimage_SRCS})
ecm_mark_nongui_executable(krootimage)
# TODO: figure out why XCB::XCB doesn't get defined so we can NOT link to ALL XCB libraries
target_link_libraries(krootimage
    KF5::CoreAddons KF5::ConfigCore
    KF5::I18n KF5::WidgetsAddons KF5::KDELibs4Support
    Qt5::Core Qt5::Widgets Qt5::Network Qt5::Svg
    ${QIMAGEBLITZ_LIBRARIES}
    Qt5::X11Extras
    ${X11_LIBRARIES} ${XCB_LIBRARIES})
install(TARGETS krootimage DESTINATION ${LIBEXEC_INSTALL_DIR})

set(genkdmconf_SRCS genkdmconf.c)
macro_add_file_dependencies(genkdmconf.c ${confci})
add_executable(genkdmconf5 ${genkdmconf_SRCS})
ecm_mark_nongui_executable(genkdmconf5)
target_compile_options(genkdmconf5 PRIVATE -U_REENTRANT)
target_link_libraries(genkdmconf5 ${X11_LIBRARIES})
add_dependencies(genkdmconf5 ConfigCi)
install(TARGETS genkdmconf5 ${INSTALL_TARGETS_DEFAULT_ARGS})

set(kdmctl_SRCS kdmctl.c)
add_executable(kdmctl5 ${kdmctl_SRCS})
ecm_mark_nongui_executable(kdmctl5)
target_compile_options(kdmctl5 PRIVATE -U_REENTRANT)
target_link_libraries(kdmctl5 ${SOCKET_LIBRARIES})
add_dependencies(kdmctl5 ConfigCi)

install(TARGETS kdmctl5 ${INSTALL_TARGETS_DEFAULT_ARGS})


# use 'GENKDMCONF_FLAGS=... make install' to add flags to the config generation (try genkdmconf5 -h)
install(CODE "
exec_program(\"${CMAKE_CURRENT_BINARY_DIR}/genkdmconf5\" ARGS --in \\\"\\\$DESTDIR${CONFIG_INSTALL_DIR}/kdm\\\" --no-in-notice --face-src \\\"${CMAKE_CURRENT_SOURCE_DIR}/pics\\\" \\\$GENKDMCONF_FLAGS)
")
