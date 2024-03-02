cmake_minimum_required(VERSION 3.18)
project(vkLearn)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# mkdir
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/ext)

#set global path
set(VKLEARN_TOP_LEVEL_DIR ${CMAKE_SOURCE_DIR})
set(VKLEARN_BINARY_DIR ${CMAKE_BINARY_DIR})

# download third-party library
add_subdirectory(ext)

execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/ext/vulkan.zip
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/ext
)

# set third-party path
set(VULKAN_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ext/vulkan)

# find vulkan
find_package(Vulkan REQUIRED)

# set path macro
add_compile_definitions(HOME_DIR="${CMAKE_SOURCE_DIR}")

add_subdirectory(src)
