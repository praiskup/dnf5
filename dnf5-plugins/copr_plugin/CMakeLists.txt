# set gettext domain for translations
add_definitions(-DGETTEXT_DOMAIN=\"dnf5_cmd_copr\")

add_library(copr_cmd_plugin MODULE copr.cpp os_release.cpp copr_cmd_plugin.cpp
            json.cpp copr_enable.cpp copr_repo.cpp copr_list.cpp copr_debug.cpp)

target_link_libraries(copr_cmd_plugin ${JSONC_LIBRARIES})
include_directories(${JSONC_INCLUDE_DIRS})

# disable the 'lib' prefix in order to create copr_cmd_plugin.so
set_target_properties(copr_cmd_plugin PROPERTIES PREFIX "")

install(TARGETS copr_cmd_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/dnf5/plugins/)
