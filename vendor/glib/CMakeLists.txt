cmake_minimum_required(VERSION 3.10)
project(glib_vendor NONE)

include(ExternalProject)
set(VENDOR_INSTALL_DIR ${CMAKE_BINARY_DIR}/vendor/glib/install)

# create the install include/lib dirs so INTERFACE paths won't be missing
file(MAKE_DIRECTORY ${VENDOR_INSTALL_DIR}/include/glib-2.0)
file(MAKE_DIRECTORY ${VENDOR_INSTALL_DIR}/lib)

ExternalProject_Add(glib
  SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}
  PREFIX            ${CMAKE_BINARY_DIR}/vendor/glib
  CONFIGURE_COMMAND meson setup
    ${CMAKE_BINARY_DIR}/vendor/glib/build
    --buildtype=release
    --default-library=static
    -Dtests=false
    --prefix=${VENDOR_INSTALL_DIR}
  BUILD_COMMAND     meson compile -C ${CMAKE_BINARY_DIR}/vendor/glib/build
  INSTALL_COMMAND   meson install -C ${CMAKE_BINARY_DIR}/vendor/glib/build
)

# expose imported static target
add_library(glib_static STATIC IMPORTED GLOBAL)
set_target_properties(glib_static PROPERTIES
  IMPORTED_LOCATION ${VENDOR_INSTALL_DIR}/lib/libglib-2.0.a
)
add_library(glib::glib ALIAS glib_static)
add_dependencies(glib_static glib)

# now attach the include dir without triggering a missing‐path check at configure time
target_include_directories(glib_static INTERFACE
  ${VENDOR_INSTALL_DIR}/include/glib-2.0
)
