# Just UI

cmake_minimum_required(VERSION 3.16)
project(JustUI VERSION 1.0 LANGUAGES C)
find_package(Gint 2.1 REQUIRED)
include(Fxconv)

set(CMAKE_INSTALL_MESSAGE LAZY)

configure_file(include/justui/config.h.in include/justui/config.h)

set(ASSETS_fx
  assets/input-modes-fx.png
)
set(ASSETS_cg
  assets/input-modes-cg.png
)
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
  src/vec.c
  src/keymap.c
  ${ASSETS_${FXSDK_PLATFORM}}
)

target_compile_options(${NAME} PUBLIC
  -Wall -Wextra -std=c11 -Os)
target_include_directories(${NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include"
  "${FXSDK_COMPILER_INSTALL}/include/openlibm")
target_link_libraries(${NAME}
  Gint::Gint -lopenlibm)

install(TARGETS ${NAME}
  DESTINATION "${FXSDK_COMPILER_INSTALL}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include"
  DESTINATION "${FXSDK_COMPILER_INSTALL}"
  FILES_MATCHING PATTERN "*.h")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/justui/config.h"
  DESTINATION "${FXSDK_COMPILER_INSTALL}/include/justui")
install(FILES cmake/FindJustUI.cmake
  DESTINATION "${FXSDK_CMAKE_MODULE_PATH}")
