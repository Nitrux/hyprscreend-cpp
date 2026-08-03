cmake_minimum_required(VERSION 3.16)
project(hyprscreend VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hyprscreend
    main.cpp
    lua_config.cpp
    display_power.cpp
)

target_compile_options(hyprscreend PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(hyprscreend PRIVATE pthread)

install(TARGETS hyprscreend RUNTIME DESTINATION bin)
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    target_compile_options(hyprscreend PRIVATE -march=x86-64-v3 -mtune=generic)
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
"if(NOT EXISTS \"${CMAKE_BINARY_DIR}/install_manifest.txt\")\n"
"    message(FATAL_ERROR \"Cannot find install manifest\")\n"
"endif()\n"
"file(READ \"${CMAKE_BINARY_DIR}/install_manifest.txt\" installed_files)\n"
"string(REPLACE \"\\n\" \";\" installed_files \"\${installed_files}\")\n"
"foreach(file \${installed_files})\n"
"    if(EXISTS \"\${file}\" OR IS_SYMLINK \"\${file}\")\n"
"        message(STATUS \"Removing \${file}\")\n"
"        file(REMOVE \"\${file}\")\n"
"    endif()\n"
"endforeach()\n")
add_custom_target(uninstall
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)
set(CPACK_PACKAGE_NAME "hyprscreend-cpp")
set(CPACK_DEBIAN_PACKAGE_NAME "hyprscreend-cpp")
set(CPACK_PACKAGE_VENDOR "Nitrux Latinoamericana S.C.")
set(CPACK_PACKAGE_CONTACT "hello@nxos.org")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Nitrux Hyprland display and power daemon")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "uri_herrera@nxos.org")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "hyprland")
set(CPACK_DEBIAN_PACKAGE_REPLACES "hyprscreend")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "hyprscreend")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)
