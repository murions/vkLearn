#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <stdexcept>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h>
#include<iostream>

const int width = 800;
const int height = 600;

class DrawApp{
public:
    void run(){
        initWindow();
        initVulkan();
        mainLoop();
        cleanUp();
    }
private:
    GLFWwindow* window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    void initWindow(){
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(width, height, "window", nullptr, nullptr);
        if(window == nullptr){
            glfwTerminate();
            throw std::runtime_error("Failed to create window!");
        }
    }

    void initVulkan(){
        createInstance();
        createPhysicalDevice();
    }

    void mainLoop(){
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanUp(){
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance(){
        // create  application info
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.pApplicationName = "Triangle";
        // create instance info
        unsigned int glfwExtensionCnt = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCnt);
        VkInstanceCreateInfo instanceCreateInfo = {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = glfwExtensionCnt;
        instanceCreateInfo.ppEnabledExtensionNames = glfwExtensions;
        instanceCreateInfo.enabledLayerCount = 0;
        // create instance
        if(vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS){
            throw std::runtime_error("Failed to create instance");
        }
    }

    void createPhysicalDevice(){
        uint32_t deviceCnt = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCnt, nullptr);
        if(deviceCnt == 0){
            throw std::runtime_error("No usable graphic device");
        }
        std::vector<VkPhysicalDevice> devices(deviceCnt);
        vkEnumeratePhysicalDevices(instance, &deviceCnt, devices.data());

        for(const auto& device : devices){
            if(isDeviceSuitable(device)){
                physicalDevice = device;
                break;
            }
        }

        if(physicalDevice == VK_NULL_HANDLE){
            throw std::runtime_error("Failed to create physical device");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice device){
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        std::cout << deviceProperties.deviceName << std::endl;
        return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }
};

int main(int argc, char* argv[]){
    DrawApp app;
    app.run();
    return 0;
}