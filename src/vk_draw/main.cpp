#include "common/vkShader.h"
#include <vector>
#include <set>

struct queueFamilyIndices{
    int graphicsFamily = -1;

    bool isComplete(){
        return graphicsFamily >= 0;
    }
};

queueFamilyIndices findQueueFamilyIndices(VkPhysicalDevice device){
    queueFamilyIndices indices;

    uint32_t familyCnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCnt, nullptr);
    std::vector<VkQueueFamilyProperties> familyProps(familyCnt);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCnt, familyProps.data());

    int i = 0;
    for(auto& prop : familyProps){
        if(prop.queueCount > 0 && prop.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;
        if(indices.isComplete())
            break;
        i++;
    }

    return indices;
}

int main(){
    // init
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "window", nullptr, nullptr);
    if(!window){
        glfwTerminate();
        throw std::runtime_error("Failed to create window");
        return -1;
    }

    // spirvHelper
    SpirvHelper spirvHelper;
    spirvHelper.Init();

    // app info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pEngineName = "No Engine";
    appInfo.pApplicationName = "VKCommand";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    // instance create info
    uint32_t extensionCnt = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCnt);
    std::vector<const char*> exts(extensions, extensions + extensionCnt);
    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = extensionCnt;
    instanceCreateInfo.ppEnabledExtensionNames = exts.data();
    std::vector<const char*> layer = {
        "VK_LAYER_KHRONOS_validation"
    };
    instanceCreateInfo.enabledLayerCount = layer.size();
    instanceCreateInfo.ppEnabledLayerNames = layer.data();

    // instance
    VkInstance instance;
    VkResult success = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create instance");
    }

    // surface
    VkSurfaceKHR surface;
    success = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create surface");
    }

    // physical device
    uint32_t physicalDeviceCnt = 0;
    VkPhysicalDevice physicalDevice;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCnt, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCnt);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCnt, physicalDevices.data());
    bool deviceStatus = false;
    for(auto& device : physicalDevices){
        VkPhysicalDeviceProperties prop = {};
        vkGetPhysicalDeviceProperties(device, &prop);
        std::cout << prop.deviceName << std::endl;
        if(prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            physicalDevice = device;
            deviceStatus = true;
            break;
        }
    }
    if(!deviceStatus){
        throw std::runtime_error("Fail to find physical device");
    }

    // queue family
    uint32_t queueFamilyCnt = 0;
    uint32_t queueFamilyGraphicsIndex = 0;
    uint32_t queueFamilyPresentIndex = 0;
    float priorities = 1.0f;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCnt, nullptr);
    if(queueFamilyCnt <= 0){
        throw std::runtime_error("Failed to find any queue family property");
    }
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCnt);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCnt, queueFamilyProps.data());
    for(auto& queueFamilyProp : queueFamilyProps){
        if(queueFamilyProp.queueFlags & VK_QUEUE_GRAPHICS_BIT){
            queueFamilyGraphicsIndex = &queueFamilyProp - queueFamilyProps.data();
            break;
        }
    }
    for(auto& queueFamilyProp : queueFamilyProps){
        int i = &queueFamilyProp - queueFamilyProps.data();
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if(presentSupport){
            queueFamilyPresentIndex = i;
            break;
        }
    }

    // queue create info
    std::set<uint32_t> queueFamilies = {queueFamilyGraphicsIndex, queueFamilyPresentIndex};
    std::vector<VkDeviceQueueCreateInfo> queueInfo;
    for(auto queueFamily : queueFamilies){
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &priorities;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueInfo.push_back(queueCreateInfo);
    }

    // logical device
    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDevice logicalDevice;
    VkDeviceCreateInfo logicalDeviceCreateInfo = {};
    logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logicalDeviceCreateInfo.queueCreateInfoCount = queueInfo.size();
    logicalDeviceCreateInfo.pQueueCreateInfos = queueInfo.data();
    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    logicalDeviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    logicalDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    logicalDeviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    success = vkCreateDevice(physicalDevice, &logicalDeviceCreateInfo, nullptr, &logicalDevice);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create logical device");
    }

    // queue
    VkQueue queue;
    vkGetDeviceQueue(logicalDevice, queueFamilyGraphicsIndex, 0, &queue);
    vkGetDeviceQueue(logicalDevice, queueFamilyPresentIndex, 0, &queue);

    // swapchain info
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    uint32_t queueFamilyIndices[] = {
        queueFamilyGraphicsIndex,
        queueFamilyPresentIndex
    };
    VkSurfaceCapabilitiesKHR surfaceCap;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCap);
    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = surfaceCap.minImageCount + 1;
    swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainInfo.imageExtent = VkExtent2D{(uint32_t)width, (uint32_t)height};
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(queueFamilyIndices[0] != queueFamilyIndices[1]){
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else{
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 0;
        swapchainInfo.pQueueFamilyIndices = nullptr;
    }
    swapchainInfo.preTransform = surfaceCap.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    swapchainInfo.clipped = VK_TRUE;

    // swapchain
    VkSwapchainKHR swapchain;
    success = vkCreateSwapchainKHR(logicalDevice, &swapchainInfo, nullptr, &swapchain);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create swapchain");
    }

    // swapchain image
    uint32_t swapchainImagesCnt = 0;
    vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImagesCnt, nullptr);
    std::vector<VkImage> swapchainImages(swapchainImagesCnt);
    vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImagesCnt, swapchainImages.data());

    // image view
    std::vector<VkImageView> swapchainImageViews(swapchainImages.size());
    for(auto i = 0; i < swapchainImageViews.size(); ++i){
        VkImageViewCreateInfo imageviewInfo = {};
        imageviewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
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
        success = vkCreateImageView(logicalDevice, &imageviewInfo, nullptr, &swapchainImageViews[i]);
        if(success != VK_SUCCESS){
            throw std::runtime_error("Failed to create imageview");
        }
    }

    // attachment description
    VkAttachmentDescription attachmentDesc = {};
    attachmentDesc.format = swapchainInfo.imageFormat;
    attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // attachment reference
    VkAttachmentReference attachmentRef = {};
    attachmentRef.attachment = 0;
    attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // subpass description
    VkSubpassDescription subpassDesc = {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &attachmentRef;

    // subpass dependency
    VkSubpassDependency subpassDependency = {};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // render pass info
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDesc;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachmentDesc;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &subpassDependency;

    // render pass
    VkRenderPass renderPass;
    success = vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create render pass");
    }

    // load shader
    std::string vsShader = "";
    std::string fsShader = "";
    if(!spirvHelper.GLSLFileLoader(HOME_DIR"/src/vk_shader/vert.vert", vsShader)){
        throw std::runtime_error("Failed to read vertex shader");
    }
    if(!spirvHelper.GLSLFileLoader(HOME_DIR"/src/vk_shader/frag.frag", fsShader)){
        throw std::runtime_error("Failed to read fragment shader");
    }

    // spirv convert
    std::vector<uint32_t> vsSPIV;
    std::vector<uint32_t> fsSPIV;
    if(!spirvHelper.GLSLtoSPV(VK_SHADER_STAGE_VERTEX_BIT, vsShader.c_str(), vsSPIV)){
        throw std::runtime_error("Failed to convert vertex glsl code to SPIRV");
    }
    if(!spirvHelper.GLSLtoSPV(VK_SHADER_STAGE_FRAGMENT_BIT, fsShader.c_str(), fsSPIV)){
        throw std::runtime_error("Failed to convert fragment code to SPIRV");
    }

    // shader module info
    VkShaderModuleCreateInfo vsShaderModuleInfo = {};
    VkShaderModuleCreateInfo fsShaderModuleInfo = {};
    vsShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vsShaderModuleInfo.codeSize = vsSPIV.size() * sizeof(uint32_t);
    vsShaderModuleInfo.pCode = vsSPIV.data();
    fsShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fsShaderModuleInfo.codeSize = fsSPIV.size() * sizeof(uint32_t);
    fsShaderModuleInfo.pCode = fsSPIV.data();

    // shader module
    VkShaderModule vsShaderModule;
    VkShaderModule fsShaderModule;
    success = vkCreateShaderModule(logicalDevice, &vsShaderModuleInfo, nullptr, &vsShaderModule);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create vertex shader module");
    }
    success = vkCreateShaderModule(logicalDevice, &fsShaderModuleInfo, nullptr, &fsShaderModule);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create fragment shader module");
    }

    // pipeline shader stage info
    VkPipelineShaderStageCreateInfo vsPipelineShaderStageInfo = {};
    VkPipelineShaderStageCreateInfo fsPipelineShaderStageInfo = {};
    vsPipelineShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsPipelineShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vsPipelineShaderStageInfo.module = vsShaderModule;
    vsPipelineShaderStageInfo.pName = "main";
    fsPipelineShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fsPipelineShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsPipelineShaderStageInfo.module = fsShaderModule;
    fsPipelineShaderStageInfo.pName = "main";
    std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageInfo = {vsPipelineShaderStageInfo, fsPipelineShaderStageInfo};
    
    // pipeline vertex input stage info
    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStageInfo = {};
    pipelineVertexInputStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStageInfo.vertexBindingDescriptionCount = 0;
    pipelineVertexInputStageInfo.pVertexBindingDescriptions = nullptr;
    pipelineVertexInputStageInfo.vertexAttributeDescriptionCount = 0;
    pipelineVertexInputStageInfo.pVertexAttributeDescriptions = nullptr;

    // pipeline input assembly state info
    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {};
    pipelineInputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineInputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

    // pipeline viewport state info
    VkViewport viewport{0.0, 0.0, (float)width, (float)height, 0.0, 1.0};
    VkRect2D scissor{{0, 0}, {uint32_t(width), uint32_t(height)}};
    VkPipelineViewportStateCreateInfo pipelineViewportStateInfo = {};
    pipelineViewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateInfo.viewportCount = 1;
    pipelineViewportStateInfo.pViewports = &viewport;
    pipelineViewportStateInfo.scissorCount = 1;
    pipelineViewportStateInfo.pScissors = &scissor;

    // pipeline rasterization state info
    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {};
    pipelineRasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
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
    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {};
    pipelineMultisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateInfo.sampleShadingEnable = VK_FALSE;
    pipelineMultisampleStateInfo.alphaToCoverageEnable = VK_FALSE;
    pipelineMultisampleStateInfo.alphaToOneEnable = VK_FALSE;
    pipelineMultisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineMultisampleStateInfo.minSampleShading  = 1.0;
    pipelineMultisampleStateInfo.pSampleMask = nullptr;

    // pipeline color blend attachment state
    VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
    pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;

    // pipeline color blend state info
    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {};
    pipelineColorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendStateInfo.attachmentCount = 1;
    pipelineColorBlendStateInfo.pAttachments = &pipelineColorBlendAttachmentState;
    pipelineColorBlendStateInfo.logicOpEnable = VK_FALSE;

    // pipeline dynamic state info
    std::vector<VkDynamicState> dynamicState = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo pipelineDynamicStateInfo = {};
    pipelineDynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateInfo.dynamicStateCount = dynamicState.size();
    pipelineDynamicStateInfo.pDynamicStates = dynamicState.data();

    // pipeline layout info
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    // pipeline layout
    VkPipelineLayout pipelineLayout;
    success = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // pipeline info
    VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {};
    graphicsPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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

    // pipeline
    VkPipeline graphicsPipeline;
    success = vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create pipeline");
    }

    // framebuffer
    std::vector<VkFramebuffer> framebuffers(swapchainImageViews.size());
    for(auto i = 0; i < framebuffers.size(); ++i){
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &swapchainImageViews[i];
        framebufferInfo.width = swapchainInfo.imageExtent.width;
        framebufferInfo.height = swapchainInfo.imageExtent.height;
        framebufferInfo.layers = 1;

        success = vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &framebuffers[i]);
        if(success != VK_SUCCESS){
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    // command pool info
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = queueFamilyGraphicsIndex;

    // command pool
    VkCommandPool commandPool;
    success = vkCreateCommandPool(logicalDevice, &commandPoolInfo, nullptr, &commandPool);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create command pool");
    }

    // command buffer
    VkCommandBuffer commandBuffer;

    // command buffer allocation
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    success = vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &commandBuffer);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate command buffer");
    }

    // synchronization
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    success = vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create semaphore");
    }
    success = vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create semaphore");
    }
    success = vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFence);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create fence");
    }

    // drawcall
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        // wait for last frame drawing operation finished
        vkWaitForFences(logicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        
        // reset fence to signal
        vkResetFences(logicalDevice, 1, &inFlightFence);
        
        // acquire image
        uint32_t acquireImageIndex = 0;
        vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &acquireImageIndex);

        // record command buffer
        vkResetCommandBuffer(commandBuffer, 0);
        
        // command buffer begin
        VkCommandBufferBeginInfo commandBufferBeginInfo = {};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;
        success = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
        if(success != VK_SUCCESS){
            throw std::runtime_error("Failed to begin recording command buffer");
        }

        // command: begin render pass
        VkClearValue clearValue{{{1.0, 0.5, 0.45, 1.0}}};
        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = framebuffers[acquireImageIndex];
        renderPassBeginInfo.renderArea.offset = VkOffset2D{0, 0};
        renderPassBeginInfo.renderArea.extent = swapchainInfo.imageExtent;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // command: bind pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        // command: set viewport
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        // command: set scissor
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // command: draw
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // command: end render pass
        vkCmdEndRenderPass(commandBuffer);

        // command buffer end
        success = vkEndCommandBuffer(commandBuffer);
        if(success != VK_SUCCESS){
            throw std::runtime_error("Failed to record command buffer");
        }

        // submit command buffer
        VkPipelineStageFlags waitDstStageMask[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = waitDstStageMask;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
        success = vkQueueSubmit(queue, 1, &submitInfo, inFlightFence);
        if(success != VK_SUCCESS){
            throw std::runtime_error("Failed to submit command buffer");
        }

        // present
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &acquireImageIndex;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        presentInfo.pResults = nullptr;
        vkQueuePresentKHR(queue, &presentInfo);
    }

    // wait
    vkDeviceWaitIdle(logicalDevice);

    // clear
    spirvHelper.Finalize();
    vkDestroyShaderModule(logicalDevice, vsShaderModule, nullptr);
    vkDestroyShaderModule(logicalDevice, fsShaderModule, nullptr);
    vkDestroySemaphore(logicalDevice, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(logicalDevice, renderFinishedSemaphore, nullptr);
    vkDestroyFence(logicalDevice, inFlightFence, nullptr);
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
    
    return 0;
}