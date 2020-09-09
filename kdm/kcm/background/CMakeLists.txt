
add_subdirectory( pics )
add_subdirectory( patterns )
add_subdirectory( programs )

if (KDM_THEMEABLE)
    install( FILES background5.knsrc  DESTINATION  ${CONFIG_INSTALL_DIR} )
endif()

