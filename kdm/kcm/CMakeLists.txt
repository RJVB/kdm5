add_subdirectory( background )

include_directories( ${QIMAGEBLITZ_INCLUDES} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/background/  )

set(kcmbackgroundlib_SRCS
    background/bgdialog.cpp
    background/bgadvanced.cpp
    background/bgmonitor.cpp
    background/bgwallpaper.cpp
)
ki18n_wrap_ui(kcmbackgroundlib_SRCS
    background/bgwallpaper_ui.ui
    background/bgdialog_ui.ui
    background/bgadvanced_ui.ui)

set(kcm_kdm_PART_SRCS ${kcmbackgroundlib_SRCS} ${backgroundlib_SRCS}
   background.cpp
   kdm-gen.cpp
   kdm-dlg.cpp
   kdm-shut.cpp
   kdm-users.cpp
   kdm-conv.cpp
   main.cpp
   kbackedcombobox.cpp
   positioner.cpp
   screenpreviewwidget.cpp)
if (KDM_THEMEABLE)
   set(kcm_kdm_PART_SRCS ${kcm_kdm_PART_SRCS}
   kdm-theme.cpp)
endif()


add_library(kcm_kdm MODULE ${kcm_kdm_PART_SRCS})

# KDE4_KIO_LIBS=KF5::KDELibs4Support
#     Qt5::Network
#     Qt5::DBus
#     Qt5::Xml
#     KF5::KIOCore
#     KF5::I18n
#     KF5::CoreAddons
#     KF5::Codecs
#     KF5::ConfigCore
#     KF5::WidgetsAddons
#     KF5::ItemModels
#     KF5::ConfigWidgets
#     KF5::Completion
#     KF5::XmlGui
#     KF5::IconThemes
#     KF5::KIOWidgets
#     KF5::ItemViews
#     KF5::Emoticons
target_link_libraries(kcm_kdm ${QIMAGEBLITZ_LIBRARIES}
    KF5::AuthCore KF5::CoreAddons KF5::Completion KF5::ConfigCore KF5::ConfigWidgets
    KF5::I18n KF5::IconThemes KF5::WidgetsAddons KF5::Plasma
    KF5::KIOCore KF5::KIOWidgets
    KF5::KDELibs4Support
    Qt5::Core Qt5::Widgets Qt5::Network Qt5::Svg Qt5::X11Extras
    ${X11_LIBRARIES})
if (KDM_THEMEABLE)
    target_link_libraries(kcm_kdm ${KDE4_KNEWSTUFF3_LIBS} kworkspace)
endif()


macro_add_file_dependencies(kdm-shut.cpp ${confci})
add_dependencies(kcm_kdm ConfigCi)

install(TARGETS kcm_kdm  DESTINATION ${PLUGIN_INSTALL_DIR})

#### KAuth helper and actions ####
add_executable(kcmkdmhelper helper.cpp)
ecm_mark_nongui_executable(kcmkdmhelper)
target_link_libraries(kcmkdmhelper
    KF5::AuthCore KF5::ConfigCore
    Qt5::Core)
install(TARGETS kcmkdmhelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})

kauth_install_helper_files(kcmkdmhelper org.kde.kcontrol.kcmkdm root)

kauth_install_actions(org.kde.kcontrol.kcmkdm kcmkdm_actions.actions)

########### install files ###############

install( FILES kdm.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

install( FILES anchor.png  DESTINATION  ${DATA_INSTALL_DIR}/kcontrol/pics )
if (KDM_THEMEABLE)
    install( FILES kdm.knsrc DESTINATION ${CONFIG_INSTALL_DIR} )
endif()
