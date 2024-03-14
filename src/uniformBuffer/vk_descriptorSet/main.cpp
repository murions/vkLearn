#include "common/vkShader.h"
#include "common/util.h"
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
        glm::vec2 pos;
        glm::vec3 col;

        static std::vector<VkVertexInputBindingDescription> getVertexInputBindingDesc(){
            std::vector<VkVertexInputBindingDescription> desc{{}};
            desc[0].binding = 0;
            desc[0].stride = sizeof(Vertex);
            desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return desc;
        }
        static std::vector<VkVertexInputAttributeDescription> getVertexInputAttribDesc(){
            std::vector<VkVertexInputAttributeDescription> desc = {{}, {}};
            desc[0].binding = 0;
            desc[0].format = VK_FORMAT_R32G32_SFLOAT;
            desc[0].location = 0;
            desc[0].offset = offsetof(Vertex, pos);
            desc[1].binding = 0;
            desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            desc[1].location = 1;
            desc[1].offset = offsetof(Vertex, col);
            return  desc;
        }
    };

    const std::vector<Vertex> vertexData{
        {{0.5, -0.5}, {1, 1, 0}},
        {{0.5, 0.5}, {1, 0, 1}},
        {{-0.5, 0.5}, {0, 1, 1}},
        {{-0.5, -0.5}, {0, 1, 0}}
    };
    const std::vector<uint16_t> vertexIndices{
        0, 1, 2,
        2, 3, 0
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

    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorSetLayout descriptorSetLayout;

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkDescriptorPool descriptorPool;

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    std::vector<VkDescriptorSet> descriptorSets{FRAMES_IN_FLIGHT};

    VkDescriptorPoolSize descriptorPoolSize = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
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

private:
    void createInstance(){
        // fill app info
        appInfo.pEngineName = "No Engine";
        appInfo.pApplicationName = "VKDescriptorSet";
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
        // update window size
        int windowCurrentWidth = 0, windowCurrentHeight = 0;
        glfwGetWindowSize(window, &windowCurrentWidth, &windowCurrentHeight);
        windowSize.width = static_cast<uint32_t>(windowCurrentWidth);
        windowSize.height = static_cast<uint32_t>(windowCurrentHeight);
        viewport.width = windowSize.width;
        viewport.height = windowSize.height;

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
        
        // fill render pass info
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDesc;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachmentDesc;
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
        descriptorSetLayoutBinding.binding = 0;
        descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        // create descriptor set layout
        descriptorSetLayoutInfo.bindingCount = 1;
        descriptorSetLayoutInfo.pBindings = &descriptorSetLayoutBinding;
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
        VK_CHECK(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline));
    }

    void createFramebuffer(){
        // create framebuffers
        framebuffers.resize(swapchainImageViews.size());
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
        VkCommandBufferAllocateInfo copyBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        copyBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        copyBufferAllocateInfo.commandPool = commandPools[1];
        copyBufferAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer copyCommandBuffer;
        VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &copyBufferAllocateInfo, &copyCommandBuffer));

        VkCommandBufferBeginInfo copyCommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        copyCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(copyCommandBuffer, &copyCommandBufferBeginInfo));

        VkBufferCopy bufferCopy;
        bufferCopy.srcOffset = 0;
        bufferCopy.dstOffset = 0;
        bufferCopy.size = size;
        vkCmdCopyBuffer(copyCommandBuffer, src, dst, 1, &bufferCopy);

        VK_CHECK(vkEndCommandBuffer(copyCommandBuffer));

        VkSubmitInfo copySubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        copySubmitInfo.commandBufferCount = 1;
        copySubmitInfo.pCommandBuffers = &copyCommandBuffer;

        VK_CHECK(vkQueueSubmit(queues[2], 1, &copySubmitInfo, VK_NULL_HANDLE));
        vkQueueWaitIdle(queues[2]);

        vkFreeCommandBuffers(logicalDevice, commandPools[1], 1, &copyCommandBuffer);
    }

    void createDescriptorPool(){
        // fill descriptor pool info
        descriptorPoolSize.descriptorCount = FRAMES_IN_FLIGHT;
        descriptorPoolInfo.poolSizeCount = 1;
        descriptorPoolInfo.pPoolSizes = &descriptorPoolSize;
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
            
            // fill write descriptor set
            VkWriteDescriptorSet writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writeDescriptorSet.dstSet = descriptorSets[i];
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.dstArrayElement = 0;
            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;

            // update descriptor set
            vkUpdateDescriptorSets(logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
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