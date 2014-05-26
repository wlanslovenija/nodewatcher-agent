cmake_minimum_required(VERSION 2.6)

project(nodewatcher-agent C)
add_definitions(-Os -Wall -Werror --std=gnu99 -Wmissing-declarations)

option(GENERAL_MODULE "General module support" ON)
option(RESOURCES_MODULE "Resources module support" ON)
option(INTERFACES_MODULE "Interfaces module support" ON)
option(WIRELESS_MODULE "Wireless module support" ON)
option(KEYS_SSH_MODULE "SSH keys module support" ON)
option(PACKAGES_MODULE "Packages module support" ON)

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

find_library(json NAMES json-c json)

set(SOURCES
  main.c
)

set(LIBS
  ubox
  ubus
  uci
  ${json}
  dl
)

include_directories(include)

# Nodewatcher agent common library
set(COMMON_SOURCES
  common/module.c
  common/scheduler.c
  common/json.c
)
add_library(nodewatcher-agent-common SHARED ${COMMON_SOURCES})
target_link_libraries(nodewatcher-agent-common ${LIBS})

# Nodewatcher agent binary
add_executable(nodewatcher-agent ${SOURCES})
target_link_libraries(nodewatcher-agent ${LIBS} nodewatcher-agent-common)

# Modules
set(MODULES "")

# General module
if(GENERAL_MODULE)
  set(MODULES ${MODULES} general_module)
  add_library(general_module MODULE modules/general.c)
  target_link_libraries(general_module ubox uci nodewatcher-agent-common)
  set_target_properties(general_module PROPERTIES OUTPUT_NAME general PREFIX "")
endif()

# Resources module
if(RESOURCES_MODULE)
  set(MODULES ${MODULES} resources_module)
  add_library(resources_module MODULE modules/resources.c)
  target_link_libraries(resources_module ubox uci nodewatcher-agent-common)
  set_target_properties(resources_module PROPERTIES OUTPUT_NAME resources PREFIX "")
endif()

# Interfaces module
if(INTERFACES_MODULE)
  set(MODULES ${MODULES} interfaces_module)
  add_library(interfaces_module MODULE modules/interfaces.c)
  target_link_libraries(interfaces_module ubox uci nodewatcher-agent-common)
  set_target_properties(interfaces_module PROPERTIES OUTPUT_NAME interfaces PREFIX "")
endif()

# Wireless module
if(WIRELESS_MODULE)
  set(MODULES ${MODULES} wireless_module)
  add_library(wireless_module MODULE modules/wireless.c)
  target_link_libraries(wireless_module ubox uci nodewatcher-agent-common)
  set_target_properties(wireless_module PROPERTIES OUTPUT_NAME wireless PREFIX "")
endif()

# SSH keys module
if(KEYS_SSH_MODULE)
  set(MODULES ${MODULES} keys_ssh_module)
  add_library(keys_ssh_module MODULE modules/keys_ssh.c)
  target_link_libraries(keys_ssh_module ubox uci nodewatcher-agent-common)
  set_target_properties(keys_ssh_module PROPERTIES OUTPUT_NAME keys_ssh PREFIX "")
endif()

# Packages module
if(PACKAGES_MODULE)
  set(MODULES ${MODULES} packages_module)
  add_library(packages_module MODULE modules/packages.c)
  target_link_libraries(packages_module ubox uci nodewatcher-agent-common)
  set_target_properties(packages_module PROPERTIES OUTPUT_NAME packages PREFIX "")
endif()

install(TARGETS nodewatcher-agent nodewatcher-agent-common ${MODULES}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)
