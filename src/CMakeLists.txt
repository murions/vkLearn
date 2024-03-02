cmake_minimum_required(VERSION 3.18)

# add single subdirectory
function(buildSubProject SUBPROJECT_NAME)
    # add subdirectory
    add_subdirectory(${SUBPROJECT_NAME})

    # add executable
    target_sources(${SUBPROJECT_NAME}
        PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/common/vkShader.cpp
            ${CMAKE_CURRENT_LIST_DIR}/common/util.cpp
    )
    
    # add include directory
    target_include_directories(${SUBPROJECT_NAME}
        PRIVATE
            ${VULKAN_INCLUDE_DIR}
            ${VKLEARN_BINARY_DIR}
            ${CMAKE_CURRENT_LIST_DIR}
    )

    # link library directory
    if(WIN32)
        target_link_libraries(${SUBPROJECT_NAME}
            PRIVATE
                glfw
                glm
                ${Vulkan_LIBRARY}
                glslang
                SPIRV
        )
    elseif(LINUX)
        target_link_libraries(${SUBPROJECT_NAME}
            PRIVATE
                glfw
                glm
                ${Vulkan_LIBRARY}
                glslang
                SPIRV
                dl
                pthread
                X11
        )
    endif()
endfunction(buildSubProject)

# add all subdirectories
function(buildSubProjects)
    foreach(SUBPROJECT ${SUBPROJECTS})
        buildSubProject(${SUBPROJECT})
    endforeach(SUBPROJECT)
endfunction(buildSubProjects)

set(SUBPROJECTS
    vk_window
    vk_instance
    vk_physicalDevice
    vk_logicalDevice
    vk_swapchain
    vk_imageView
    vk_shader
    vk_fixedFunctions
    vk_renderPass
    vk_pipeline
    vk_framebuffer
    vk_commandBuffer
    vk_draw
)

buildSubProjects()
