
include_directories( ${KDM_SOURCE_DIR}/kdm/kfrontend  )

########### next target ###############

set(kgreet_classic_PART_SRCS kgreet_classic.cpp )


add_library(kgreet_classic MODULE ${kgreet_classic_PART_SRCS})


target_link_libraries(kgreet_classic
    KF5::CoreAddons KF5::Completion KF5::I18n Qt5::Core Qt5::Gui)

install(TARGETS kgreet_classic  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### next target ###############

set(kgreet_winbind_PART_SRCS kgreet_winbind.cpp )


add_library(kgreet_winbind MODULE ${kgreet_winbind_PART_SRCS})


target_link_libraries(kgreet_winbind
    KF5::CoreAddons KF5::Completion KF5::I18n Qt5::Core Qt5::Gui)

install(TARGETS kgreet_winbind  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### next target ###############

set(kgreet_generic_PART_SRCS kgreet_generic.cpp )


add_library(kgreet_generic MODULE ${kgreet_generic_PART_SRCS})


target_link_libraries(kgreet_generic
    KF5::CoreAddons KF5::Completion KF5::I18n Qt5::Core Qt5::Gui)

install(TARGETS kgreet_generic  DESTINATION ${PLUGIN_INSTALL_DIR} )





########### install files ###############

install( FILES kgreeterplugin.h DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/libkdm COMPONENT Devel )
