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
target_link_libraries(kdm_greet ${KDE4_KDEUI_LIBS} ${QT_QTXML_LIBRARY} ${X11_X11_LIB} ${POSIX4_LIBRARIES})
if (X11_XTest_FOUND)
  target_link_libraries(kdm_greet ${X11_XTest_LIB})
endif (X11_XTest_FOUND)
if (WITH_KDM_XCONSOLE)
  target_link_libraries(kdm_greet ${KDE4_KPTY_LIBS})
endif (WITH_KDM_XCONSOLE)

install(TARGETS kdm_greet DESTINATION ${LIBEXEC_INSTALL_DIR})

set(kdm_config_SRCS kdm_config.c)
macro_add_file_dependencies(kdm_config.c ${confci})
add_executable(kdm_config ${kdm_config_SRCS})
ecm_mark_nongui_executable(kdm_config)
target_compile_options(kdm_config PRIVATE -U_REENTRANT)
target_link_libraries(kdm_config ${SOCKET_LIBRARIES} ${RESOLV_LIBRARIES} ${POSIX4_LIBRARIES})
add_dependencies(kdm_config ConfigCi)
install(TARGETS kdm_config DESTINATION ${LIBEXEC_INSTALL_DIR})

set(krootimage_SRCS krootimage.cpp krootimage.h ${backgroundlib_SRCS})
add_executable(krootimage ${krootimage_SRCS})
ecm_mark_nongui_executable(krootimage)
target_link_libraries(krootimage ${KDE4_KDEUI_LIBS} ${X11_X11_LIB} ${QIMAGEBLITZ_LIBRARIES})
install(TARGETS krootimage DESTINATION ${LIBEXEC_INSTALL_DIR})

set(genkdmconf_SRCS genkdmconf.c)
macro_add_file_dependencies(genkdmconf.c ${confci})
add_executable(genkdmconf ${genkdmconf_SRCS})
ecm_mark_nongui_executable(genkdmconf)
target_compile_options(genkdmconf PRIVATE -U_REENTRANT)
target_link_libraries(genkdmconf ${X11_LIBRARIES})
add_dependencies(genkdmconf ConfigCi)
install(TARGETS genkdmconf ${INSTALL_TARGETS_DEFAULT_ARGS})

set(kdmctl_SRCS kdmctl.c)
add_executable(kdmctl ${kdmctl_SRCS})
ecm_mark_nongui_executable(kdmctl)
target_compile_options(kdmctl PRIVATE -U_REENTRANT)
target_link_libraries(kdmctl ${SOCKET_LIBRARIES})
add_dependencies(kdmctl ConfigCi)

install(TARGETS kdmctl ${INSTALL_TARGETS_DEFAULT_ARGS})


# use 'GENKDMCONF_FLAGS=... make install' to add flags to the config generation (try genkdmconf -h)
install(CODE "
exec_program(\"${CMAKE_CURRENT_BINARY_DIR}/genkdmconf\" ARGS --in \\\"\\\$DESTDIR${CONFIG_INSTALL_DIR}/kdm\\\" --no-in-notice --face-src \\\"${CMAKE_CURRENT_SOURCE_DIR}/pics\\\" \\\$GENKDMCONF_FLAGS)
")
