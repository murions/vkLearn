#define VK_CHECK(call)\
{\
    VkResult success = call;\
    if(success != VK_SUCCESS){\
        char errorMsg[1024];\
        sprintf(errorMsg, "Failed to call %s, with code %d (line %d).\n", #call, success, __LINE__);\
        throw std::runtime_error(errorMsg);\
    }\
}

#define VK_EXPECT_TRUE(value, msg)\
{\
    if(value != true)\
        throw std::runtime_error(msg);\
}
