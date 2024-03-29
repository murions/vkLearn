#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <set>
#include <vector>

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

    // app info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pEngineName = "No Engine";
    appInfo.pApplicationName = "VKSwapchain";
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
        #ifdef VK_ENABLE_VALIDATION_LAYER
            "VK_LAYER_KHRONOS_validation"
        #endif
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

    // drawcall
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
    }

    // clear
    vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}