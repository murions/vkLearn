cmake_minimum_required(VERSION 3.14)
project(vk_device)

# add executable
add_executable(${PROJECT_NAME} main.cpp)

# link library
if(WIN32)
	find_package(Vulkan REQUIRED)
	target_link_libraries(${PROJECT_NAME} glfw3 ${Vulkan_LIBRARY})
elseif(LINUX)
	find_package(Vulkan REQUIRED)
	target_link_libraries(${PROJECT_NAME} glfw3 ${Vulkan_LIBRARY} dl pthread X11)
endif()