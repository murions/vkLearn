cmake_minimum_required(VERSION 3.18)
project(vk_instance)

# add executable
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
	PRIVATE
		main.cpp
)
