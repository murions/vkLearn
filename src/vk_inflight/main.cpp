#include "common/vkShader.h"
#include "common/util.h"
#include <cstdint>
#include <set>
#include <iostream>
#include <vector>

#ifndef FRAMES_IN_FLIGHT
    #define FRAMES_IN_FLIGHT 2
#endif

VKShader shader(CURRENT_FILE_DIR"/vert.vert", CURRENT_FILE_DIR"/frag.frag");

VkExtent2D windowSize{800u, 600u};
VkViewport viewport{0.0, 0.0, (float)windowSize.width, (float)windowSize.height, 0.0, 1.0};
VkRect2D scissor{{0, 0}, windowSize};

int main(){
    // init
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(windowSize.width, windowSize.height, "window", nullptr, nullptr);
    VK_EXPECT_TRUE(window != nullptr, "Failed to create window.")

    // app info
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    { // fill app info
        appInfo.pEngineName = "No Engine";
        appInfo.pApplicationName = "VKInflight";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    }

    // instance info
    VkInstanceCreateInfo instanceInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    { // fill instance info
        uint32_t extensionCnt = 0;
        static const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCnt);
        static std::vector<const char*> layer = {"VK_LAYER_KHRONOS_validation"};
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = extensionCnt;
        instanceInfo.ppEnabledExtensionNames = extensions;
        instanceInfo.enabledLayerCount = layer.size();
        instanceInfo.ppEnabledLayerNames = layer.data();
    }

    // instance
    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));

    // surface
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

    // physical device
    VkPhysicalDevice physicalDevice;
    { // choose physical device
        uint32_t physicalDeviceCnt = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCnt, nullptr));
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCnt);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCnt, physicalDevices.data()));
        uint32_t deviceIndex = -1;
        std::vector<VkPhysicalDeviceProperties> prop(physicalDeviceCnt);
        for(auto i = 0; i < physicalDeviceCnt; ++i){
            vkGetPhysicalDeviceProperties(physicalDevices[i], &prop[i]);
            if(prop[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
                deviceIndex = i;
                break;
            }
            if(i == physicalDeviceCnt - 1)
                deviceIndex = 0;
        }
        VK_EXPECT_TRUE(deviceIndex >= 0, "Fail to find physical device.");
        std::cout << "Device: " << prop[deviceIndex].deviceName << std::endl;
        physicalDevice = physicalDevices[deviceIndex];
    }

    // queue family
    std::vector<uint32_t> queueFamilyIndices = {UINT32_MAX, UINT32_MAX}; // [0] is graphics index, [1] is present index
    { // choose graphics and present queue family
        uint32_t queueFamilyCnt = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCnt, nullptr);
        VK_EXPECT_TRUE(queueFamilyCnt > 0, "Failed to find any queue family property.");
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCnt);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCnt, queueFamilyProps.data());
        for(auto i = 0; i < queueFamilyProps.size(); ++i){
            if((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && queueFamilyIndices[0] == UINT32_MAX)
                queueFamilyIndices[0] = i;

            VkBool32 presentSupport = false;
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport));
            if(presentSupport && queueFamilyIndices[1] == UINT32_MAX)
                queueFamilyIndices[1] = i;
        }
    }

    // queue info
    std::vector<VkDeviceQueueCreateInfo> queueInfo;
    std::set<uint32_t> queueFamilies = {queueFamilyIndices[0], queueFamilyIndices[1]}; // remove the same
    float queuePriorities = 1.0f;
    { // fill queue info
        for(auto queueFamily : queueFamilies){
            VkDeviceQueueCreateInfo queueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriorities;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueInfo.push_back(queueCreateInfo);
        }
    }

    // logical device info
    VkDeviceCreateInfo logicalDeviceInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    { // fill logical device info
        static VkPhysicalDeviceFeatures deviceFeatures = {};
        static const std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        logicalDeviceInfo.queueCreateInfoCount = queueInfo.size();
        logicalDeviceInfo.pQueueCreateInfos = queueInfo.data();
        logicalDeviceInfo.enabledExtensionCount = deviceExtensions.size();
        logicalDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        logicalDeviceInfo.pEnabledFeatures = &deviceFeatures;
    }

    // logical device
    VkDevice logicalDevice;
    VK_CHECK(vkCreateDevice(physicalDevice, &logicalDeviceInfo, nullptr, &logicalDevice));

    // queue
    VkQueue queue;
    vkGetDeviceQueue(logicalDevice, queueFamilyIndices[0], 0, &queue);
    vkGetDeviceQueue(logicalDevice, queueFamilyIndices[1], 0, &queue);

    // swapchain info
    VkSwapchainCreateInfoKHR swapchainInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    { // fill swapchain info
        VkSurfaceCapabilitiesKHR surfaceCap;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCap);
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = surfaceCap.minImageCount + 1;
        swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchainInfo.imageExtent = windowSize;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if(queueFamilyIndices[0] != queueFamilyIndices[1]){
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchainInfo.queueFamilyIndexCount = 2;
            swapchainInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }
        else{
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchainInfo.queueFamilyIndexCount = 0;
        }
        swapchainInfo.preTransform = surfaceCap.currentTransform;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        swapchainInfo.clipped = VK_TRUE;
    }

    // swapchain
    VkSwapchainKHR swapchain;
    VK_CHECK(vkCreateSwapchainKHR(logicalDevice, &swapchainInfo, nullptr, &swapchain));

    // swapchain image
    std::vector<VkImage> swapchainImages;
    { // get swapchain image
        uint32_t swapchainImagesCnt = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImagesCnt, nullptr));
        swapchainImages.resize(swapchainImagesCnt);
        VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImagesCnt, swapchainImages.data()));
    }

    // image view
    std::vector<VkImageView> swapchainImageViews(swapchainImages.size());
    { // create swapchain image view
        for(auto i = 0; i < swapchainImageViews.size(); ++i){
            VkImageViewCreateInfo imageviewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            imageviewInfo.image = swapchainImages[i];
            imageviewInfo.format = swapchainInfo.imageFormat;
            imageviewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageviewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageviewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageviewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageviewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageviewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageviewInfo.subresourceRange.baseArrayLayer = 0;
            imageviewInfo.subresourceRange.baseMipLevel = 0;
            imageviewInfo.subresourceRange.layerCount = 1;
            imageviewInfo.subresourceRange.levelCount = 1;
            VK_CHECK(vkCreateImageView(logicalDevice, &imageviewInfo, nullptr, &swapchainImageViews[i]));
        }
    }

    // render pass info
    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    { // fill render pass info
        // attachment description
        static VkAttachmentDescription attachmentDesc = {};
        attachmentDesc.format = swapchainInfo.imageFormat;
        attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // attachment reference
        static VkAttachmentReference attachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // subpass description
        static VkSubpassDescription subpassDesc = {};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &attachmentRef;

        // subpass dependency
        static VkSubpassDependency subpassDependency = {};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.srcAccessMask = 0;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDesc;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachmentDesc;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &subpassDependency;
    }

    // render pass
    VkRenderPass renderPass;
    VK_CHECK(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass));

    // shader module info
    VkShaderModuleCreateInfo vsShaderModuleInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkShaderModuleCreateInfo fsShaderModuleInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    { // fill shader module info
        vsShaderModuleInfo.codeSize = shader.vs_size;
        vsShaderModuleInfo.pCode = shader.vs_spirv.data();
        fsShaderModuleInfo.codeSize = shader.fs_size;
        fsShaderModuleInfo.pCode = shader.fs_spirv.data();
    }

    // shader module
    VkShaderModule vsShaderModule;
    VkShaderModule fsShaderModule;
    VK_CHECK(vkCreateShaderModule(logicalDevice, &vsShaderModuleInfo, nullptr, &vsShaderModule));
    VK_CHECK(vkCreateShaderModule(logicalDevice, &fsShaderModuleInfo, nullptr, &fsShaderModule));

    // pipeline layout info
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    { // fill pipeline layout info
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
    }

    // pipeline layout
    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    // pipeline info
    VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    { // fill pipeline infos
        // pipeline shader stage info
        static std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageInfo{
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}
        };
        pipelineShaderStageInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        pipelineShaderStageInfo[0].module = vsShaderModule;
        pipelineShaderStageInfo[0].pName = "main";
        pipelineShaderStageInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipelineShaderStageInfo[1].module = fsShaderModule;
        pipelineShaderStageInfo[1].pName = "main";

        // pipeline vertex input stage info
        static VkPipelineVertexInputStateCreateInfo pipelineVertexInputStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        pipelineVertexInputStageInfo.vertexBindingDescriptionCount = 0;
        pipelineVertexInputStageInfo.vertexAttributeDescriptionCount = 0;

        // pipeline input assembly state info
        static VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        pipelineInputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineInputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

        // pipeline viewport state info
        static VkPipelineViewportStateCreateInfo pipelineViewportStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        pipelineViewportStateInfo.viewportCount = 1;
        pipelineViewportStateInfo.pViewports = &viewport;
        pipelineViewportStateInfo.scissorCount = 1;
        pipelineViewportStateInfo.pScissors = &scissor;

        // pipeline rasterization state info
        static VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        pipelineRasterizationStateInfo.depthClampEnable = VK_FALSE;
        pipelineRasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
        pipelineRasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        pipelineRasterizationStateInfo.lineWidth = 1.0;
        pipelineRasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        pipelineRasterizationStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        pipelineRasterizationStateInfo.depthBiasEnable = VK_FALSE;
        pipelineRasterizationStateInfo.depthBiasClamp = 0.0;
        pipelineRasterizationStateInfo.depthBiasConstantFactor = 0.0;
        pipelineRasterizationStateInfo.depthBiasSlopeFactor = 0.0;

        // pipeline mutisample state info
        static VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        pipelineMultisampleStateInfo.sampleShadingEnable = VK_FALSE;
        pipelineMultisampleStateInfo.alphaToCoverageEnable = VK_FALSE;
        pipelineMultisampleStateInfo.alphaToOneEnable = VK_FALSE;
        pipelineMultisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipelineMultisampleStateInfo.minSampleShading  = 1.0;
        pipelineMultisampleStateInfo.pSampleMask = nullptr;

        // pipeline color blend attachment state
        static VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
        pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;

        // pipeline color blend state info
        static VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        pipelineColorBlendStateInfo.attachmentCount = 1;
        pipelineColorBlendStateInfo.pAttachments = &pipelineColorBlendAttachmentState;
        pipelineColorBlendStateInfo.logicOpEnable = VK_FALSE;

        // pipeline dynamic state info
        static VkPipelineDynamicStateCreateInfo pipelineDynamicStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        static std::vector<VkDynamicState> dynamicState = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        pipelineDynamicStateInfo.dynamicStateCount = dynamicState.size();
        pipelineDynamicStateInfo.pDynamicStates = dynamicState.data();

        // pipeline info
        graphicsPipelineInfo.renderPass = renderPass;
        graphicsPipelineInfo.subpass = 0;
        graphicsPipelineInfo.stageCount = pipelineShaderStageInfo.size();
        graphicsPipelineInfo.pStages = pipelineShaderStageInfo.data();
        graphicsPipelineInfo.pVertexInputState = &pipelineVertexInputStageInfo;
        graphicsPipelineInfo.pInputAssemblyState = &pipelineInputAssemblyStateInfo;
        graphicsPipelineInfo.pViewportState = &pipelineViewportStateInfo;
        graphicsPipelineInfo.pRasterizationState = &pipelineRasterizationStateInfo;
        graphicsPipelineInfo.pMultisampleState = &pipelineMultisampleStateInfo;
        graphicsPipelineInfo.pColorBlendState = &pipelineColorBlendStateInfo;
        graphicsPipelineInfo.pDynamicState = &pipelineDynamicStateInfo;
        graphicsPipelineInfo.layout = pipelineLayout;
    }

    // pipeline
    VkPipeline graphicsPipeline;
    VK_CHECK(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline));

    // framebuffer
    std::vector<VkFramebuffer> framebuffers(swapchainImageViews.size());
    { // create framebuffers
        for(auto i = 0; i < framebuffers.size(); ++i){
            VkFramebufferCreateInfo framebufferInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &swapchainImageViews[i];
            framebufferInfo.width = windowSize.width;
            framebufferInfo.height = windowSize.height;
            framebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &framebuffers[i]));
        }
    }

    // command pool info
    VkCommandPoolCreateInfo commandPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    { // fill command pool info
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = queueFamilyIndices[0];
    }

    // command pool
    VkCommandPool commandPool;
    VK_CHECK(vkCreateCommandPool(logicalDevice, &commandPoolInfo, nullptr, &commandPool));

    // command buffer allocate info
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    { // fill command buffer allocate info
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.commandBufferCount = FRAMES_IN_FLIGHT;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    }

    // command buffer
    std::vector<VkCommandBuffer> commandBuffers(FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, commandBuffers.data()));

    // synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores(FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> renderFinishedSemaphores(FRAMES_IN_FLIGHT);
    std::vector<VkFence> inFlightFences(FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for(int i = 0; i < FRAMES_IN_FLIGHT; ++i){
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
        VK_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]));
    }

    // drawcall
    uint32_t currentFrame = 0;
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        // wait for last frame drawing operation finished
        vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        
        // reset fence to signal
        vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
        
        // acquire image
        uint32_t acquireImageIndex = 0;
        vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &acquireImageIndex);
        
        // record command buffer
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        // begin command buffer
        VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        VK_CHECK(vkBeginCommandBuffer(commandBuffers[currentFrame], &commandBufferBeginInfo));

        // render pass begin info
        VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        { // fill render pass begin info
            static VkClearValue clearValue{{{1.0, 0.5, 0.45, 1.0}}};
            renderPassBeginInfo.renderPass = renderPass;
            renderPassBeginInfo.framebuffer = framebuffers[acquireImageIndex];
            renderPassBeginInfo.renderArea.offset = VkOffset2D{0, 0};
            renderPassBeginInfo.renderArea.extent = swapchainInfo.imageExtent;
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = &clearValue;
        }

        // do render pass
        vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);
        vkCmdDraw(commandBuffers[currentFrame], 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffers[currentFrame]);

        // end command buffer
        VK_CHECK(vkEndCommandBuffer(commandBuffers[currentFrame]));

        // submit command buffer
        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        { // fill submit info and submit
            static VkPipelineStageFlags waitDstStageMask[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
            submitInfo.pWaitDstStageMask = waitDstStageMask;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];
            VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, inFlightFences[currentFrame]));
        }

        // present
        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        { // fill present info and preset
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &acquireImageIndex;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
            VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));
        }

        // update current frame
        currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;
    }

    // wait for closing
    VK_CHECK(vkDeviceWaitIdle(logicalDevice));

    // clear
    { // clean up resources
        vkDestroyShaderModule(logicalDevice, vsShaderModule, nullptr);
        vkDestroyShaderModule(logicalDevice, fsShaderModule, nullptr);
        for(int i = 0; i < FRAMES_IN_FLIGHT; ++i){
            vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
        }
        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
        for(auto &framebuffer : framebuffers)
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
        vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
        for(auto &imageview : swapchainImageViews)
            vkDestroyImageView(logicalDevice, imageview, nullptr);
        vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
        vkDestroyDevice(logicalDevice, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    return 0;
}