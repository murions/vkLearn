//vkShader.h

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "glslang/Include/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"

struct SpirvHelper
{
public:
	void Init();

	void Finalize();

	bool GLSLFileLoader(std::string path, std::string& shaderSource);
	
	bool GLSLtoSPV(const VkShaderStageFlagBits shader_type, const char *pshader, std::vector<uint32_t> &spirv);

private:
	void InitResources(TBuiltInResource &Resources);

	EShLanguage FindLanguage(const VkShaderStageFlagBits shader_type);
};