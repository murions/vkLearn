#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
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
    appInfo.pApplicationName = "VKCommand";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    // instance create info
    uint32_t extensionCnt = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCnt);
    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = extensionCnt;
    instanceCreateInfo.ppEnabledExtensionNames = extensions;
    instanceCreateInfo.enabledLayerCount = 0;

    // instance
    VkInstance instance;
    VkResult success = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create instance");
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
    uint32_t queueFamilyIndex = 0;
    float priorities = 1.0f;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCnt, nullptr);
    if(queueFamilyCnt <= 0){
        throw std::runtime_error("Failed to find any queue family property");
    }
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCnt);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCnt, queueFamilyProps.data());
    for(auto& queueFamilyProp : queueFamilyProps){
        if(queueFamilyProp.queueFlags & VK_QUEUE_GRAPHICS_BIT){
            queueFamilyIndex = &queueFamilyProp - queueFamilyProps.data();
            break;
        }
    }

    // queue create info
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priorities;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;

    // logical device
    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDevice logicalDevice;
    VkDeviceCreateInfo logicalDeviceCreateInfo = {};
    logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logicalDeviceCreateInfo.queueCreateInfoCount = 1;
    logicalDeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    logicalDeviceCreateInfo.enabledExtensionCount = 0;
    logicalDeviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    success = vkCreateDevice(physicalDevice, &logicalDeviceCreateInfo, nullptr, &logicalDevice);
    if(success != VK_SUCCESS){
        throw std::runtime_error("Failed to create logical device");
    }

    // queue
    VkQueue queue;
    vkGetDeviceQueue(logicalDevice, queueFamilyIndex, 0, &queue);

    // drawcall
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
    }

    // clear
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}