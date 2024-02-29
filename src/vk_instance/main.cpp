#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

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

    void initWindow(){
        glfwInit();
        glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwInitHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(width, height, "window", nullptr, nullptr);
        if(window == nullptr){
            glfwTerminate();
            throw std::runtime_error("Failed to create window!");
        }
    }

    void initVulkan(){
        createInstance();
    }
    
    void mainLoop(){
        while(!glfwWindowShouldClose(window)){
            glfwPollEvents();
        }
    }

    void cleanUp(){
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance(){
        // create application info
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pEngineName = "Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        // create instance info
        unsigned int glfwExtensionCnt = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCnt);
        VkInstanceCreateInfo instanceCrtInfo = {};
        instanceCrtInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCrtInfo.pApplicationInfo = &appInfo;
        instanceCrtInfo.enabledExtensionCount = glfwExtensionCnt;
        instanceCrtInfo.ppEnabledExtensionNames = glfwExtensions;
        instanceCrtInfo.enabledLayerCount = 0;
        // create instance
        VkResult result = vkCreateInstance(&instanceCrtInfo, nullptr, &instance);
        if(result != VK_SUCCESS){
            throw std::runtime_error("Failed to create instance!");
        }
    }
};

int main(){
    DrawApp app;

    app.run();

    return 0;
}