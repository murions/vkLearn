#include "common/vkShader.h"
#include "common/util.h"
#include "vulkan/vulkan_core.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <set>
#include <iostream>
#include <vector>
#include <chrono>

#ifndef FRAMES_IN_FLIGHT
    #define FRAMES_IN_FLIGHT 2
#endif

class App{
private:
    GLFWwindow* window;
    VKShader shader{CURRENT_FILE_DIR"/vert.vert", CURRENT_FILE_DIR"/frag.frag"};

    VkExtent2D windowSize{800u, 600u};
    VkViewport viewport{0.0, 0.0, (float)windowSize.width, (float)windowSize.height, 0.0, 1.0};
    VkRect2D scissor{{0, 0}, windowSize};
    
    float queuePriorities = 1.0f;

    bool framebufferResized = false;

    VkDeviceSize offsets = {0};

    struct Vertex{
        glm::vec3 pos;
        glm::vec3 col;
        glm::vec2 texcoord;

        static std::vector<VkVertexInputBindingDescription> getVertexInputBindingDesc(){
            std::vector<VkVertexInputBindingDescription> desc{{}};
            desc[0].binding = 0;
            desc[0].stride = sizeof(Vertex);
            desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return desc;
        }
        static std::vector<VkVertexInputAttributeDescription> getVertexInputAttribDesc(){
            std::vector<VkVertexInputAttributeDescription> desc{3};
            desc[0].binding = 0;
            desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            desc[0].location = 0;
            desc[0].offset = offsetof(Vertex, pos);
            desc[1].binding = 0;
            desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            desc[1].location = 1;
            desc[1].offset = offsetof(Vertex, col);
            desc[2].binding = 0;
            desc[2].format = VK_FORMAT_R32G32_SFLOAT;
            desc[2].location = 2;
            desc[2].offset = offsetof(Vertex, texcoord);
            return  desc;
        }
    };

    const std::vector<Vertex> vertexData{
        {{0.5, -0.5, 0.25}, {1, 1, 0}, {1, 1}},
        {{0.5, 0.5, 0.25}, {1, 0, 1}, {1, 0}},
        {{-0.5, 0.5, 0.25}, {0, 1, 1}, {0, 0}},
        {{-0.5, -0.5, 0.25}, {0, 1, 0}, {0, 1}},

        {{0.5, -0.5, -0.25}, {1, 1, 0}, {1, 1}},
        {{0.5, 0.5, -0.25}, {1, 0, 1}, {1, 0}},
        {{-0.5, 0.5, -0.25}, {0, 1, 1}, {0, 0}},
        {{-0.5, -0.5, -0.25}, {0, 1, 0}, {0, 1}}
    };
    const std::vector<uint16_t> vertexIndices{
        0, 1, 2,
        2, 3, 0,

        4, 5, 6,
        6, 7, 4
    };

    struct Uniform{
        alignas(4) float padding;
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

public:
    void run(){
        // drawcall
        uint32_t currentFrame = 0;
        while(!glfwWindowShouldClose(window)){
            glfwPollEvents();

            // wait for last frame drawing operation finished
            VK_CHECK(vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX));
            
            // acquire image
            uint32_t acquireImageIndex = 0;
            VkResult windowResult = vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &acquireImageIndex);
            if(windowResult == VK_ERROR_OUT_OF_DATE_KHR){
                recreateSwapchain();
                return;
            }
            else if (windowResult != VK_SUCCESS && windowResult != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("Failed to acquire swapchain image.");
            }
            
            // reset fence to signal
            VK_CHECK(vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]));

            // update uniform buffer
            updateUniformData(currentFrame);
            
            // record command buffer
            VK_CHECK(vkResetCommandBuffer(commandBuffers[currentFrame], 0));

            // begin command buffer
            VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            VK_CHECK(vkBeginCommandBuffer(commandBuffers[currentFrame], &commandBufferBeginInfo));

            // render pass begin info
            VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            { // fill render pass begin info
                static std::vector<VkClearValue> clearValues{{{1.0, 0.5, 0.45, 1.0}}, {1.0f, 0.0f}};
                renderPassBeginInfo.renderPass = renderPass;
                renderPassBeginInfo.framebuffer = framebuffers[acquireImageIndex];
                renderPassBeginInfo.renderArea.offset = VkOffset2D{0, 0};
                renderPassBeginInfo.renderArea.extent = swapchainInfo.imageExtent;
                renderPassBeginInfo.clearValueCount = clearValues.size();
                renderPassBeginInfo.pClearValues = clearValues.data();
            }

            // do render pass
            vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);
            vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);
            vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, &vertexBuffer, &offsets);
            vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
            vkCmdDrawIndexed(commandBuffers[currentFrame], vertexIndices.size(), 1, 0, 0, 0);
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
                VK_CHECK(vkQueueSubmit(queues[0], 1, &submitInfo, inFlightFences[currentFrame]));
            }

            // present
            VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            { // fill present info and preset
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = &swapchain;
                presentInfo.pImageIndices = &acquireImageIndex;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
                windowResult = vkQueuePresentKHR(queues[1], &presentInfo);
            }

            if(windowResult == VK_ERROR_OUT_OF_DATE_KHR || windowResult == VK_SUBOPTIMAL_KHR || framebufferResized){
                framebufferResized = false;
                recreateSwapchain();
            }
            else if (windowResult != VK_SUCCESS) {
                throw std::runtime_error("Failed to acquire swapchain image.");
            }

            // update current frame
            currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;
        }
    }

public:
    App(){
        // init
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(windowSize.width, windowSize.height, "window", nullptr, nullptr);
        VK_EXPECT_TRUE(window != nullptr, "Failed to create window.")

        // resize callback
        glfwSetWindowUserPointer(window, this);
        glfwSetWindowSizeCallback(window, resizeCallback);

        // instance
        createInstance();

        // surface
        VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

        // physical device
        createPhysicalDevice();

        // logical device
        createLogicalDevice();

        // queue
        vkGetDeviceQueue(logicalDevice, queueFamilyIndices[0], 0, &queues[0]);
        vkGetDeviceQueue(logicalDevice, queueFamilyIndices[1], 0, &queues[1]);
        vkGetDeviceQueue(logicalDevice, queueFamilyIndices[2], 0, &queues[2]);

        // swapchain
        createSwapchain();

        // swapchain image view
        createSwapchainImageView();

        // render pass
        createRenderPass();

        // shader module
        createShaderModule();

        // descriptor set layout
        createDescriptorSetLayout();

        // pipeline layout
        createPipelineLayout();

        // pipeline
        createPipeline();

        // depth image
        createDepthImage();

        // framebuffer
        createFramebuffer();

        // command pool
        createCommandPool();

        // vertex buffer
        allocateVertexBuffer();

        // vertex index
        allocateVertexIndex();

        // uniform buffer
        allocateUniformBuffer();

        // texture image
        createTextureImage();

        // texture sampler
        createTextureSampler();

        // descriptor pool
        createDescriptorPool();

        // descriptor set
        createDescriptorSet();

        // command buffer
        allocateCommandBuffer();

        // synchronization
        createSyncObjects();
    }

    ~App(){
        // clear
        cleanup();
    }


private:
    // internal objects or object infos
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    VkInstanceCreateInfo instanceInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkInstance instance;

    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice;
    std::vector<uint32_t> queueFamilyIndices{UINT32_MAX, UINT32_MAX, UINT32_MAX}; // [0]: graphics, [1]: present, [2]: transfer
    
    std::vector<VkDeviceQueueCreateInfo> queueInfo;
    std::set<uint32_t> queueFamilies;
    VkDeviceCreateInfo logicalDeviceInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    VkDevice logicalDevice;
    std::vector<VkQueue> queues{3}; // [0]: graphics, [1]: present, [2]: transfer 
    
    VkSwapchainCreateInfoKHR swapchainInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    VkSwapchainKHR swapchain;

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    
    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    VkRenderPass renderPass;
    
    VkShaderModuleCreateInfo vsShaderModuleInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkShaderModuleCreateInfo fsShaderModuleInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkShaderModule vsShaderModule;
    VkShaderModule fsShaderModule;

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings{2};
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorSetLayout descriptorSetLayout;

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkDescriptorPool descriptorPool;

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    std::vector<VkDescriptorSet> descriptorSets{FRAMES_IN_FLIGHT};

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes{2};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkPipelineLayout pipelineLayout;
    
    VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    VkPipeline graphicsPipeline;

    std::vector<VkFramebuffer> framebuffers;
    
    std::vector<VkCommandPoolCreateInfo> commandPoolInfos{2, {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}};
    std::vector<VkCommandPool> commandPools{2};
    
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    std::vector<VkCommandBuffer> commandBuffers{FRAMES_IN_FLIGHT};
    
    VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    std::vector<VkSemaphore> imageAvailableSemaphores{FRAMES_IN_FLIGHT};
    std::vector<VkSemaphore> renderFinishedSemaphores{FRAMES_IN_FLIGHT};
    std::vector<VkFence> inFlightFences{FRAMES_IN_FLIGHT};

    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VkDeviceMemory vertexMemory;
    VkDeviceMemory indexMemory;

    std::vector<VkBuffer> uniformBuffer{FRAMES_IN_FLIGHT};
    std::vector<VkDeviceMemory> uniformMemory{FRAMES_IN_FLIGHT};
    std::vector<void*> uniformData{FRAMES_IN_FLIGHT};

    VkImage image;
    VkDeviceMemory imageMemory;
    VkImageView imageView;
    VkSampler sampler;

    VkImage depthImage;
    VkDeviceMemory depthMemory;
    VkImageView depthView;

private:
    void createInstance(){
        // fill app info
        appInfo.pEngineName = "No Engine";
        appInfo.pApplicationName = "VKDepth";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        // fill instance info
        uint32_t extensionCnt = 0;
        static const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCnt);
        static std::vector<const char*> layer = {
        #ifdef VK_ENABLE_VALIDATION_LAYER
            "VK_LAYER_KHRONOS_validation"
        #endif
            };
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = extensionCnt;
        instanceInfo.ppEnabledExtensionNames = extensions;
        instanceInfo.enabledLayerCount = layer.size();
        instanceInfo.ppEnabledLayerNames = layer.data();
        VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));
    }

    void createPhysicalDevice(){
        // choose physical device
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

        // choose graphics and present queue family
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
            
            if(((queueFamilyProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && !(queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) && queueFamilyIndices[2] == UINT32_MAX)
                queueFamilyIndices[2] = i;
        }
    }

    void createLogicalDevice(){
        // fill queue info
        queueFamilies.insert(queueFamilyIndices[0]);
        queueFamilies.insert(queueFamilyIndices[1]);
        queueFamilies.insert(queueFamilyIndices[2]); // remove the same
        for(auto queueFamily : queueFamilies){
            VkDeviceQueueCreateInfo queueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriorities;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueInfo.push_back(queueCreateInfo);
        }
        
        // fill logical device info
        static VkPhysicalDeviceFeatures deviceFeatures = {};
        static const std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        logicalDeviceInfo.queueCreateInfoCount = queueInfo.size();
        logicalDeviceInfo.pQueueCreateInfos = queueInfo.data();
        logicalDeviceInfo.enabledExtensionCount = deviceExtensions.size();
        logicalDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        logicalDeviceInfo.pEnabledFeatures = &deviceFeatures;
        VK_CHECK(vkCreateDevice(physicalDevice, &logicalDeviceInfo, nullptr, &logicalDevice));
    }

    void createSwapchain(){
        // fill swapchain info
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
        VK_CHECK(vkCreateSwapchainKHR(logicalDevice, &swapchainInfo, nullptr, &swapchain));
    }

    void createSwapchainImageView(){ 
        // get swapchain image
        uint32_t swapchainImagesCnt = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImagesCnt, nullptr));
        swapchainImages.resize(swapchainImagesCnt);
        VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImagesCnt, swapchainImages.data()));
        swapchainImageViews.resize(swapchainImagesCnt);

        // create swapchain image view
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

    void createRenderPass(){
        // attachment description
        static std::vector<VkAttachmentDescription> attachmentDescs{2};
        attachmentDescs[0].format = swapchainInfo.imageFormat;
        attachmentDescs[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachmentDescs[1].format = VK_FORMAT_D32_SFLOAT;
        attachmentDescs[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // attachment reference
        static std::vector<VkAttachmentReference> attachmentRefs{2};
        attachmentRefs[0].attachment = 0;
        attachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentRefs[1].attachment = 1;
        attachmentRefs[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // subpass description
        static VkSubpassDescription subpassDesc = {};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &attachmentRefs[0];
        subpassDesc.pDepthStencilAttachment = &attachmentRefs[1];

        // subpass dependency
        static VkSubpassDependency subpassDependency = {};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpassDependency.srcAccessMask = 0;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        
        // fill render pass info
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDesc;
        renderPassInfo.attachmentCount = attachmentDescs.size();
        renderPassInfo.pAttachments = attachmentDescs.data();
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &subpassDependency;
        VK_CHECK(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass));
    }

    void createShaderModule(){
        // fill shader module info
        vsShaderModuleInfo.codeSize = shader.vs_size;
        vsShaderModuleInfo.pCode = shader.vs_spirv.data();
        fsShaderModuleInfo.codeSize = shader.fs_size;
        fsShaderModuleInfo.pCode = shader.fs_spirv.data();
        VK_CHECK(vkCreateShaderModule(logicalDevice, &vsShaderModuleInfo, nullptr, &vsShaderModule));
        VK_CHECK(vkCreateShaderModule(logicalDevice, &fsShaderModuleInfo, nullptr, &fsShaderModule));
    }

    void createDescriptorSetLayout(){
        // fill descriptor set layout info
        descriptorSetLayoutBindings[0].binding = 0;
        descriptorSetLayoutBindings[0].descriptorCount = 1;
        descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        descriptorSetLayoutBindings[1].binding = 1;
        descriptorSetLayoutBindings[1].descriptorCount = 1;
        descriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // create descriptor set layout
        descriptorSetLayoutInfo.bindingCount = descriptorSetLayoutBindings.size();
        descriptorSetLayoutInfo.pBindings = descriptorSetLayoutBindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));
    }

    void createPipelineLayout(){
        // fill pipeline layout info
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout));
    }

    void createPipeline(){
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
        static auto vertexBindingDesc = Vertex::getVertexInputBindingDesc();
        static auto vertexAttribDesc = Vertex::getVertexInputAttribDesc();
        pipelineVertexInputStageInfo.vertexBindingDescriptionCount = vertexBindingDesc.size();
        pipelineVertexInputStageInfo.vertexAttributeDescriptionCount = vertexAttribDesc.size();
        pipelineVertexInputStageInfo.pVertexBindingDescriptions = vertexBindingDesc.data();
        pipelineVertexInputStageInfo.pVertexAttributeDescriptions = vertexAttribDesc.data();

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
        pipelineRasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

        // pipeline depth stencil state info
        static VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        pipelineDepthStencilStateInfo.depthTestEnable = VK_TRUE;
        pipelineDepthStencilStateInfo.depthWriteEnable = VK_TRUE;
        pipelineDepthStencilStateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        pipelineDepthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
        pipelineDepthStencilStateInfo.stencilTestEnable = VK_FALSE;

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
        graphicsPipelineInfo.pDepthStencilState = &pipelineDepthStencilStateInfo;
        graphicsPipelineInfo.layout = pipelineLayout;
        VK_CHECK(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline));
    }

    void createFramebuffer(){
        // create framebuffers
        framebuffers.resize(swapchainImageViews.size());
        for(auto i = 0; i < framebuffers.size(); ++i){
            std::vector<VkImageView> attachs = {swapchainImageViews[i], depthView};
            VkFramebufferCreateInfo framebufferInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = attachs.size();
            framebufferInfo.pAttachments = attachs.data();
            framebufferInfo.width = windowSize.width;
            framebufferInfo.height = windowSize.height;
            framebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &framebuffers[i]));
        }
    }

    void createCommandPool(){
        // graphics command pool
        commandPoolInfos[0].flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfos[0].queueFamilyIndex = queueFamilyIndices[0];
        VK_CHECK(vkCreateCommandPool(logicalDevice, &commandPoolInfos[0], nullptr, &commandPools[0]));

        // transfer command pool
        commandPoolInfos[1].flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfos[1].queueFamilyIndex = queueFamilyIndices[2];
        VK_CHECK(vkCreateCommandPool(logicalDevice, &commandPoolInfos[1], nullptr, &commandPools[1]));
    }

    void allocateVertexBuffer(){
        VkDeviceSize size = sizeof(vertexData[0]) * vertexData.size();
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), stagingBuffer, stagingMemory);
        
        // map memory
        void* data;
        vkMapMemory(logicalDevice, stagingMemory, offsets, size, 0, &data);
        memcpy(data, vertexData.data(), size);
        vkUnmapMemory(logicalDevice, stagingMemory);
        
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), vertexBuffer, vertexMemory);
        
        copyBuffer(stagingBuffer, vertexBuffer, size);

        // clean
        vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(logicalDevice, stagingMemory, nullptr);
    }

    void allocateVertexIndex(){
        VkDeviceSize size = sizeof(vertexIndices[0]) * vertexIndices.size();
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), stagingBuffer, stagingMemory);
        
        // map memory
        void* data;
        vkMapMemory(logicalDevice, stagingMemory, offsets, size, 0, &data);
        memcpy(data, vertexIndices.data(), size);
        vkUnmapMemory(logicalDevice, stagingMemory);
        
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), indexBuffer, indexMemory);
        
        copyBuffer(stagingBuffer, indexBuffer, size);

        // clean
        vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(logicalDevice, stagingMemory, nullptr);
    }

    void allocateUniformBuffer(){
        VkDeviceSize size = sizeof(Uniform);

        for(auto i = 0; i < FRAMES_IN_FLIGHT; ++i){
            createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VkMemoryPropertyFlagBits(
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
                uniformBuffer[i], uniformMemory[i]);

            vkMapMemory(logicalDevice, uniformMemory[i], offsets, size, 0, &uniformData[i]);
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlagBits props, VkBuffer& buffer, VkDeviceMemory& memory){
        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.usage = usage;
        bufferInfo.size = size;
        bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        std::vector<uint32_t> indices = {queueFamilyIndices[0], queueFamilyIndices[2]};
        bufferInfo.queueFamilyIndexCount = 2;
        bufferInfo.pQueueFamilyIndices = indices.data();
        
        // create vertex buffer
        VK_CHECK(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer));

        // get memory properties
        VkPhysicalDeviceMemoryProperties memoryProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProps);

        // get memory requirements
        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);
        
        // get memory type index
        uint32_t memoryTypeIndex = 0;
        for(auto i = 0; i < memoryProps.memoryTypeCount; ++i){
            if((memoryRequirements.memoryTypeBits & (1 << i)) && (memoryProps.memoryTypes[i].propertyFlags & props) == props){
                memoryTypeIndex = i;
                break;
            }
        }    

        // fill memory allocate info
        VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

        // allocate memory
        VK_CHECK(vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr, &memory));

        // bind vertex buffer
        VK_CHECK(vkBindBufferMemory(logicalDevice, buffer, memory, offsets));
    }

    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size){
        // begin
        VkCommandBufferAllocateInfo copyBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        copyBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        copyBufferAllocateInfo.commandPool = commandPools[1];
        copyBufferAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer copyCommandBuffer;
        VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &copyBufferAllocateInfo, &copyCommandBuffer));

        VkCommandBufferBeginInfo copyCommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        copyCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(copyCommandBuffer, &copyCommandBufferBeginInfo));

        // copy
        VkBufferCopy bufferCopy;
        bufferCopy.srcOffset = 0;
        bufferCopy.dstOffset = 0;
        bufferCopy.size = size;
        vkCmdCopyBuffer(copyCommandBuffer, src, dst, 1, &bufferCopy);

        // end
        VK_CHECK(vkEndCommandBuffer(copyCommandBuffer));

        VkSubmitInfo copySubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        copySubmitInfo.commandBufferCount = 1;
        copySubmitInfo.pCommandBuffers = &copyCommandBuffer;

        VK_CHECK(vkQueueSubmit(queues[2], 1, &copySubmitInfo, VK_NULL_HANDLE));
        vkQueueWaitIdle(queues[2]);

        vkFreeCommandBuffers(logicalDevice, commandPools[1], 1, &copyCommandBuffer);
    }

    void createTextureImage(){
        // load image
        int textureWidth, textureHeight, textureChannel;
        unsigned char* img = stbi_load(HOME_DIR"/img/statue.jpg", &textureWidth, &textureHeight, &textureChannel, STBI_rgb_alpha);
        
        // staging buffer
        VkDeviceSize size = textureWidth * textureHeight * 4;
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VkMemoryPropertyFlagBits(
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            ), stagingBuffer, stagingMemory);
        
        void* data;
        vkMapMemory(logicalDevice, stagingMemory, 0, size, 0, &data);
        memcpy(data, img, size);
        vkUnmapMemory(logicalDevice, stagingMemory);

        stbi_image_free(img);

        // create image
        createImage2D(textureWidth, textureHeight, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, image, imageMemory);
        
        // copy buffer
        copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight), 1);

        // clean
        vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(logicalDevice, stagingMemory, nullptr);

        // image view
        createImageView(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, imageView);
    }

    void createDepthImage(){
        createImage2D(windowSize.width, windowSize.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImage, depthMemory);
        createImageView(depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, depthView);
    }
    
    void createImage2D(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage &image, VkDeviceMemory &memory){
        // fill image info
        VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = VkExtent3D{width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        // create image
        VK_CHECK(vkCreateImage(logicalDevice, &imageInfo, nullptr, &image));
        
        // get memory properties
        VkPhysicalDeviceMemoryProperties memoryProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProps);

        // get memory requirements
        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(logicalDevice, image, &memoryRequirements);
        
        // get memory type index
        uint32_t memoryTypeIndex = 0;
        VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        for(auto i = 0; i < memoryProps.memoryTypeCount; ++i){
            if((memoryRequirements.memoryTypeBits & (1 << i)) && (memoryProps.memoryTypes[i].propertyFlags & props) == props){
                memoryTypeIndex = i;
                break;
            }
        }    

        // fill memory allocate info
        VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

        // allocate memory
        VK_CHECK(vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr, &memory));

        // bind vertex buffer
        VK_CHECK(vkBindImageMemory(logicalDevice, image, memory, 0));
    }

    void copyBufferToImage(VkBuffer src, VkImage dst, uint32_t width, uint32_t height, uint32_t depth){
        // begin
        VkCommandBufferAllocateInfo copyBufferAllocateInfo1 = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        copyBufferAllocateInfo1.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        copyBufferAllocateInfo1.commandPool = commandPools[1];
        copyBufferAllocateInfo1.commandBufferCount = 1;

        VkCommandBuffer copyCommandBuffer1;
        VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &copyBufferAllocateInfo1, &copyCommandBuffer1));

        VkCommandBufferBeginInfo copyCommandBufferBeginInfo1 = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        copyCommandBufferBeginInfo1.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(copyCommandBuffer1, &copyCommandBufferBeginInfo1));

        // image memory barrier first
        VkImageMemoryBarrier imageMemoryBarrierSrc = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageMemoryBarrierSrc.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrierSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrierSrc.srcAccessMask = 0;
        imageMemoryBarrierSrc.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrierSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrierSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrierSrc.image = image;
        imageMemoryBarrierSrc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrierSrc.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrierSrc.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrierSrc.subresourceRange.layerCount = 1;
        imageMemoryBarrierSrc.subresourceRange.levelCount = 1;
        vkCmdPipelineBarrier(copyCommandBuffer1,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrierSrc
        );

        // copy
        VkBufferImageCopy bufferImageCopy;
        bufferImageCopy.bufferOffset = 0;
        bufferImageCopy.bufferRowLength = 0;
        bufferImageCopy.bufferImageHeight = 0;
        bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferImageCopy.imageSubresource.mipLevel = 0;
        bufferImageCopy.imageSubresource.baseArrayLayer = 0;
        bufferImageCopy.imageSubresource.layerCount = 1;
        bufferImageCopy.bufferOffset = 0;
        bufferImageCopy.imageExtent = VkExtent3D{width, height, depth};
        vkCmdCopyBufferToImage(copyCommandBuffer1, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

        // end
        VK_CHECK(vkEndCommandBuffer(copyCommandBuffer1));

        VkSubmitInfo copySubmitInfo1 = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        copySubmitInfo1.commandBufferCount = 1;
        copySubmitInfo1.pCommandBuffers = &copyCommandBuffer1;

        VK_CHECK(vkQueueSubmit(queues[2], 1, &copySubmitInfo1, VK_NULL_HANDLE));
        vkQueueWaitIdle(queues[2]);

        vkFreeCommandBuffers(logicalDevice, commandPools[1], 1, &copyCommandBuffer1);

        // begin
        VkCommandBufferAllocateInfo copyBufferAllocateInfo2 = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        copyBufferAllocateInfo2.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        copyBufferAllocateInfo2.commandPool = commandPools[0];
        copyBufferAllocateInfo2.commandBufferCount = 1;

        VkCommandBuffer copyCommandBuffer2;
        VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &copyBufferAllocateInfo2, &copyCommandBuffer2));

        VkCommandBufferBeginInfo copyCommandBufferBeginInfo2 = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        copyCommandBufferBeginInfo2.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(copyCommandBuffer2, &copyCommandBufferBeginInfo2));

        // image memory barrier last
        VkImageMemoryBarrier imageMemoryBarrierDst = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageMemoryBarrierDst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrierDst.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrierDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrierDst.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrierDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrierDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrierDst.image = image;
        imageMemoryBarrierDst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrierDst.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrierDst.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrierDst.subresourceRange.layerCount = 1;
        imageMemoryBarrierDst.subresourceRange.levelCount = 1;
        vkCmdPipelineBarrier(copyCommandBuffer2,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrierDst
        );

        // end
        VK_CHECK(vkEndCommandBuffer(copyCommandBuffer2));

        VkSubmitInfo copySubmitInfo2 = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        copySubmitInfo2.commandBufferCount = 1;
        copySubmitInfo2.pCommandBuffers = &copyCommandBuffer2;

        VK_CHECK(vkQueueSubmit(queues[0], 1, &copySubmitInfo2, VK_NULL_HANDLE));
        vkQueueWaitIdle(queues[0]);

        vkFreeCommandBuffers(logicalDevice, commandPools[0], 1, &copyCommandBuffer2);
    }

    void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, VkImageView &imageView){
        // fill image view info
        VkImageViewCreateInfo imageViewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image = image;
        imageViewInfo.format = format;
        imageViewInfo.subresourceRange.aspectMask = aspect;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.subresourceRange.levelCount = 1;

        // create image view
        VK_CHECK(vkCreateImageView(logicalDevice, &imageViewInfo, nullptr, &imageView));
    }

    void createTextureSampler(){
        // fill texture sampler
        VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0;
        samplerInfo.mipLodBias = 0.0;
        samplerInfo.maxLod = 0.0;
        samplerInfo.minLod = 0.0;

        // create texture sampler
        VK_CHECK(vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &sampler));
    }
    
    void createDescriptorPool(){
        // fill descriptor pool info
        descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSizes[0].descriptorCount = FRAMES_IN_FLIGHT;
        descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorPoolSizes[1].descriptorCount = FRAMES_IN_FLIGHT;
        descriptorPoolInfo.poolSizeCount = descriptorPoolSizes.size();
        descriptorPoolInfo.pPoolSizes = descriptorPoolSizes.data();
        descriptorPoolInfo.maxSets = FRAMES_IN_FLIGHT;

        // create descriptor pool
        VK_CHECK(vkCreateDescriptorPool(logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
    }

    void createDescriptorSet(){
        // fill descriptor set allocate info
        static std::vector<VkDescriptorSetLayout> descriptorSetLayouts(FRAMES_IN_FLIGHT, descriptorSetLayout);
        descriptorSetAllocateInfo.descriptorPool = descriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = FRAMES_IN_FLIGHT;
        descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

        // allocate descriptor set
        VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &descriptorSetAllocateInfo, descriptorSets.data()));

        // populate descriptor set
        for(auto i = 0; i < FRAMES_IN_FLIGHT; ++i){
            // fill descriptor buffer info
            VkDescriptorBufferInfo descriptorBufferInfo;
            descriptorBufferInfo.buffer = uniformBuffer[i];
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range = sizeof(Uniform);

            // fill descriptor image info
            VkDescriptorImageInfo descriptorImageInfo;
            descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo.sampler = sampler;
            descriptorImageInfo.imageView = imageView;
            
            // fill write descriptor set
            std::vector<VkWriteDescriptorSet> writeDescriptorSets(2, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET});
            writeDescriptorSets[0].dstSet = descriptorSets[i];
            writeDescriptorSets[0].dstBinding = 0;
            writeDescriptorSets[0].dstArrayElement = 0;
            writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSets[0].descriptorCount = 1;
            writeDescriptorSets[0].pBufferInfo = &descriptorBufferInfo;
            writeDescriptorSets[1].dstSet = descriptorSets[i];
            writeDescriptorSets[1].dstBinding = 1;
            writeDescriptorSets[1].dstArrayElement = 0;
            writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSets[1].descriptorCount = 1;
            writeDescriptorSets[1].pImageInfo = &descriptorImageInfo;

            // update descriptor set
            vkUpdateDescriptorSets(logicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
        }
    }

    void allocateCommandBuffer(){
        // fill command buffer allocate info
        commandBufferAllocateInfo.commandPool = commandPools[0];
        commandBufferAllocateInfo.commandBufferCount = FRAMES_IN_FLIGHT;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, commandBuffers.data()));
    }

    void createSyncObjects(){
        // deal sync
        for(int i = 0; i < FRAMES_IN_FLIGHT; ++i){
            VK_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
            VK_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
            VK_CHECK(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]));
        }
    }

    void updateUniformData(uint32_t currentFrame){
        static auto start = std::chrono::high_resolution_clock::now();

        auto end = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(end - start).count();

        // set uniform data
        Uniform uniform;
        uniform.model = glm::rotate(glm::identity<glm::mat4>(), time * glm::radians(90.0f), glm::vec3(0, 0, 1));
        uniform.view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
        uniform.proj = glm::perspective(glm::radians(45.0f), windowSize.width / (float)windowSize.height, 0.1f, 10.0f);
        uniform.proj[1][1] *= -1;
        memcpy(uniformData[currentFrame], &uniform, sizeof(uniform));
    }

    void recreateSwapchain(){
        // get current window size
        int windowCurrentWidth = 0, windowCurrentHeight = 0;
        glfwGetWindowSize(window, &windowCurrentWidth, &windowCurrentHeight);
        while (windowCurrentWidth == 0 || windowCurrentHeight == 0) {
            glfwGetWindowSize(window, &windowCurrentWidth, &windowCurrentHeight);
            glfwWaitEvents();
        }

        // update window size
        glfwGetWindowSize(window, &windowCurrentWidth, &windowCurrentHeight);
        windowSize.width = static_cast<uint32_t>(windowCurrentWidth);
        windowSize.height = static_cast<uint32_t>(windowCurrentHeight);
        viewport.width = windowSize.width;
        viewport.height = windowSize.height;
        scissor.extent = windowSize;

        // wait
        vkDeviceWaitIdle(logicalDevice);

        // clean up
        cleanupSwapchain();
        
        // recreate swapchain
        createSwapchain();
        createSwapchainImageView();
        createRenderPass();
        createPipelineLayout();
        createPipeline();
        createDepthImage();
        createFramebuffer();
        allocateCommandBuffer();
    }

    void cleanupSwapchain(){
        // cleanup swapchain
        for(auto i = 0; i < framebuffers.size(); ++i)
            vkDestroyFramebuffer(logicalDevice, framebuffers[i], nullptr);
        vkFreeCommandBuffers(logicalDevice, commandPools[0], commandBuffers.size(), commandBuffers.data());
        vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
        vkDestroyImageView(logicalDevice, depthView, nullptr);
        vkDestroyImage(logicalDevice, depthImage, nullptr);
        vkFreeMemory(logicalDevice, depthMemory, nullptr);
        for(auto i = 0; i < swapchainImages.size(); ++i)
            vkDestroyImageView(logicalDevice, swapchainImageViews[i], nullptr);
        vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
    }

    void cleanup(){
        // wait for closing
        VK_CHECK(vkDeviceWaitIdle(logicalDevice));
        // clean up resources
        vkDestroyShaderModule(logicalDevice, vsShaderModule, nullptr);
        vkDestroyShaderModule(logicalDevice, fsShaderModule, nullptr);
        for(int i = 0; i < FRAMES_IN_FLIGHT; ++i){
            vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
        }
        cleanupSwapchain();
        vkDestroySampler(logicalDevice, sampler, nullptr);
        vkDestroyImageView(logicalDevice, imageView, nullptr);
        vkDestroyImage(logicalDevice, image, nullptr);
        vkFreeMemory(logicalDevice, imageMemory, nullptr);
        for(auto i = 0; i < FRAMES_IN_FLIGHT; ++i){
            vkDestroyBuffer(logicalDevice, uniformBuffer[i], nullptr);
            vkFreeMemory(logicalDevice, uniformMemory[i], nullptr);
        }
        vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
        vkFreeMemory(logicalDevice, vertexMemory, nullptr);
        vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
        vkFreeMemory(logicalDevice, indexMemory, nullptr);
        vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
        vkDestroyCommandPool(logicalDevice, commandPools[0], nullptr);
        vkDestroyCommandPool(logicalDevice, commandPools[1], nullptr);
        vkDestroyDevice(logicalDevice, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    static void resizeCallback(GLFWwindow* window, int width, int height){
        App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }
};

int main(){
    App app;
    try{
        app.run();
    }catch(const std::exception& e){
        std::cout << e.what() << std::endl;
        return -1;
    }

    return 0;
}