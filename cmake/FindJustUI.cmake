if(FXSDK_PLATFORM STREQUAL "exword")
  find_package(Gint 2.11 REQUIRED)
  find_library(JUSTUI_EXWORD_LIBRARY NAMES justui-exword PATH_SUFFIXES lib)
  find_path(JUSTUI_EXWORD_INCLUDE_DIR NAMES justui/config.h PATH_SUFFIXES include)
  include(FindPackageHandleStandardArgs)
  if(JUSTUI_EXWORD_INCLUDE_DIR)
    file(STRINGS "${JUSTUI_EXWORD_INCLUDE_DIR}/justui/config.h" J_VERSION_LINE
      REGEX "^#define J_VERSION")
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" J_VERSION "${J_VERSION_LINE}")
  endif()
  find_package_handle_standard_args(JustUI
    REQUIRED_VARS JUSTUI_EXWORD_LIBRARY JUSTUI_EXWORD_INCLUDE_DIR
    VERSION_VAR J_VERSION)
  if(JustUI_FOUND AND NOT TARGET JustUI::JustUI)
    add_library(JustUI::JustUI UNKNOWN IMPORTED)
    set_target_properties(JustUI::JustUI PROPERTIES
      IMPORTED_LOCATION "${JUSTUI_EXWORD_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${JUSTUI_EXWORD_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "Gint::Gint;m")
  endif()
  return()
endif()

include(FindSimpleLibrary)
include(FindPackageHandleStandardArgs)

find_package(Gint 2.1 REQUIRED)

find_simple_library("libjustui-${FXSDK_PLATFORM}.a" justui/config.h
  "J_VERSION" PATH_VAR J_PATH VERSION_VAR J_VERSION)

find_package_handle_standard_args(JustUI
  REQUIRED_VARS J_PATH J_VERSION
  VERSION_VAR J_VERSION)

if(JustUI_FOUND)
  add_library(JustUI::JustUI UNKNOWN IMPORTED)
  set_target_properties(JustUI::JustUI PROPERTIES
    IMPORTED_LOCATION "${J_PATH}"
    INTERFACE_LINK_OPTIONS -ljustui-${FXSDK_PLATFORM}
    IMPORTED_LINK_INTERFACE_LIBRARIES Gint::Gint)
endif()
