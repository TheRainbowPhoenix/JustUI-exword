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
