# Just UI

cmake_minimum_required(VERSION 3.16)
project(JustUI VERSION 1.3.0 LANGUAGES C)
find_package(Gint 2.8 REQUIRED)
include(Fxconv)

set(CMAKE_INSTALL_MESSAGE LAZY)

configure_file(include/justui/config.h.in include/justui/config.h)

set(ASSETS_fx
  assets/input-modes-fx.png
  assets/font-fkeys-fx.png
)
set(ASSETS_cg
  assets/input-modes-cg.png
)
set(ASSETS_cp ${ASSETS_cg})
fxconv_declare_assets(${ASSETS_fx} ${ASSETS_cg} WITH_METADATA)

set(NAME "justui-${FXSDK_PLATFORM}")
add_library(${NAME} STATIC
  src/jwidget.c
  src/jlayout_box.c
  src/jlayout_stack.c
  src/jlayout_grid.c
  src/jlabel.c
  src/jscene.c
  src/jinput.c
  src/jpainted.c
  src/jfkeys.c
  src/jfileselect.c
  src/jframe.c
  src/jlist.c
  src/jscrolledlist.c
  src/vec.c
  src/keymap.c
  ${ASSETS_${FXSDK_PLATFORM}}
)

target_compile_options(${NAME} PUBLIC
  -Wall -Wextra -std=c11 -Os -flto)
target_include_directories(${NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include")
target_link_libraries(${NAME}
  Gint::Gint -lm)

# Generate the archive with gcc-ar instead of ar as it will load the LTO plugin
# which is required to generate a usable archive.
set(CMAKE_C_ARCHIVE_CREATE "${CMAKE_C_COMPILER_AR} qcs <TARGET> <OBJECTS>")
# Also the ranlib rule (useless because ar is passed the s flag anyway)
set(CMAKE_C_ARCHIVE_FINISH "${CMAKE_C_COMPILER_RANLIB} <TARGET>")

install(TARGETS ${NAME}
  DESTINATION "${FXSDK_LIB}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  DESTINATION "${FXSDK_INCLUDE}"
  FILES_MATCHING PATTERN "*.h")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/justui/config.h"
  DESTINATION "${FXSDK_INCLUDE}/justui")
install(FILES cmake/FindJustUI.cmake
  DESTINATION "${FXSDK_CMAKE_MODULE_PATH}")
