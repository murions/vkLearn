cmake_minimum_required(VERSION 3.14)
project(vk_framebuffer)

# add executable
add_executable(${PROJECT_NAME} main.cpp)

# add include directory
target_include_directories(${PROJECT_NAME}
	PRIVATE
		${VULKAN_INCLUDE_DIRECTORY}
		${VKLEARN_BINARY_DIRECTORY}
		${CMAKE_CURRENT_LIST_DIR}
)

find_package(Vulkan REQUIRED)

# link library directory
if(WIN32)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE
			glfw
			glm
			${Vulkan_LIBRARY}
			glslang
	)
elseif(LINUX)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE
			glfw
			glm
			${Vulkan_LIBRARY}
			glslang
			dl
			pthread
			X11
	)
endif()