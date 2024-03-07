cmake_minimum_required(VERSION 3.16)
project(vk_fixedFunctions)

# add executable
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
	PRIVATE
		main.cpp
)
