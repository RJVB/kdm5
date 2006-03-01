# first try to find kde-config
# then ask kde-config for the kde data dirs
# then check the data dirs for FindKDE4.cmake

MACRO(_MACRO_GETENV_WIN_PATH var name)
   set(${var} $ENV{${name}})
   STRING( REGEX REPLACE "\\\\" "/" ${var} "${${var}}" )
ENDMACRO(_MACRO_GETENV_WIN_PATH)

_MACRO_GETENV_WIN_PATH(ENV_KDEDIR KDEDIR)


FIND_PROGRAM(KDE4_KDECONFIG_EXECUTABLE NAMES kde-config
   PATHS
   ${ENV_KDEDIR}/bin
   /opt/kde4/bin
   /opt/kde
   )


IF (KDE4_KDECONFIG_EXECUTABLE)

   EXEC_PROGRAM(${KDE4_KDECONFIG_EXECUTABLE} ARGS --path data OUTPUT_VARIABLE _data_DIR )

   # replace the ":" with ";" so that it becomes a valid cmake list
   STRING(REGEX REPLACE ":" ";" _data_DIR "${_data_DIR}")

   MESSAGE(STATUS "datadir: ${_data_DIR}")

   FIND_PATH(KDE4_DATA_DIR cmake/modules/FindKDE4Internal.cmake 
      ${_data_DIR}
   )

   IF (KDE4_DATA_DIR)

      SET(CMAKE_MODULE_PATH  ${KDE4_DATA_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

      IF (KDE4_FIND_QUIETLY)
         SET(_quiet QUIET)
      ENDIF (KDE4_FIND_QUIETLY)

      IF (KDE4_FIND_REQUIRED)
         SET(_req REQUIRED)
      ENDIF (KDE4_FIND_REQUIRED)

      FIND_PACKAGE(KDE4Internal ${_req} ${_quiet})

   ENDIF (KDE4_DATA_DIR)


ENDIF (KDE4_KDECONFIG_EXECUTABLE)


IF (KDE4_FIND_REQUIRED AND NOT KDE4_FOUND)
   MESSAGE(FATAL_ERROR "Could not find KDE4")
ENDIF (KDE4_FIND_REQUIRED AND NOT KDE4_FOUND)


# these are not good
# they should be named KDE4_KIO_LIBRARIES etc
# and moved to kdelibs/cmake/modules/FindKDE4Internal.cmake
# Alex

set(LIB_KDECORE ${QT_AND_KDECORE_LIBS} ${QT_QTGUI_LIBRARY} ${X11_X11_LIB} DCOP ${ZLIB_LIBRARY})

set(LIB_KDEUI ${LIB_KDECORE} kdeui )

set(LIB_KIO ${LIB_KDEUI} kio)

set(LIB_KPARTS ${LIB_KIO} kparts)

set(LIB_KUTILS ${LIB_KPARTS} kutils)

set(LIB_KDE3SUPPORT ${QT_QT3SUPPORT_LIBRARY} ${LIB_KUTILS})


