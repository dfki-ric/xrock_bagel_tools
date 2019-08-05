project(xrock_bagel_tools)
set(PROJECT_VERSION 1.0)
set(PROJECT_DESCRIPTION "...")
cmake_minimum_required(VERSION 2.6)
include(FindPkgConfig)

find_package(lib_manager)
lib_defaults()

pkg_check_modules(PKGCONFIG REQUIRED
          configmaps
          mars_utils
          lib_manager
)

include_directories(${PKGCONFIG_INCLUDE_DIRS})
link_directories(${PKGCONFIG_LIBRARY_DIRS})
add_definitions(${PKGCONFIG_CFLAGS_OTHER})  #flags excluding the ones with -I

include_directories(
	src
)

add_executable(get_unreferenced_models src/get_unreferenced_models.cpp)
target_link_libraries(get_unreferenced_models
                      ${PKGCONFIG_LIBRARIES}
)

add_executable(model_to_bagel src/model_to_bagel.cpp)
target_link_libraries(model_to_bagel
                      ${PKGCONFIG_LIBRARIES}
)

add_executable(list_depending_models src/list_depending_models.cpp)
target_link_libraries(list_depending_models
                      ${PKGCONFIG_LIBRARIES}
)

add_executable(bagel_model_info src/bagel_model_info.cpp)
target_link_libraries(bagel_model_info
                      ${PKGCONFIG_LIBRARIES}
)

if(WIN32)
  set(LIB_INSTALL_DIR bin) # .dll are in PATH, like executables
else(WIN32)
  set(LIB_INSTALL_DIR lib)
endif(WIN32)


set(_INSTALL_DESTINATIONS
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION ${LIB_INSTALL_DIR}
	ARCHIVE DESTINATION lib
)

# Install the library into the lib folder
install(TARGETS get_unreferenced_models ${_INSTALL_DESTINATIONS})
install(TARGETS model_to_bagel ${_INSTALL_DESTINATIONS})
install(TARGETS list_depending_models ${_INSTALL_DESTINATIONS})
install(TARGETS bagel_model_info ${_INSTALL_DESTINATIONS})

# Install the binaries to the bin folder
INSTALL(PROGRAMS bin/add_to_bagel_db DESTINATION bin)
INSTALL(PROGRAMS bin/recreate_info_yml DESTINATION bin)
INSTALL(PROGRAMS bin/bagel DESTINATION bin)
INSTALL(PROGRAMS bin/create_mars_config DESTINATION bin)
INSTALL(PROGRAMS bin/init_parameters_to_graph DESTINATION bin)
INSTALL(PROGRAMS bin/start_mars DESTINATION bin)
