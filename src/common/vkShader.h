//vkShader.h

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "glslang/Include/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include <vector>
#include <string>

struct SpirvHelper
{
public:
	static void Init();

	static void Finalize();

	bool GLSLFileLoader(std::string path, std::string& shaderSource);

	bool createVFShader(std::string vsPath, std::string fsPath, std::vector<uint32_t>& vsSPIRV, std::vector<uint32_t>& fsSPIRV);

private:
	void InitResources(TBuiltInResource &Resources);

	EShLanguage FindLanguage(const VkShaderStageFlagBits shader_type);
	
	bool GLSLtoSPV(const VkShaderStageFlagBits shader_type, const char *pshader, std::vector<uint32_t>& spirv);
};

class VKShader {
private:
	static size_t objectCnt;

	SpirvHelper spirvHelper;

	std::string vs_path;
	std::string fs_path;

	bool getVFShader(std::vector<uint32_t>& vs, std::vector<uint32_t>& fs){
		return spirvHelper.createVFShader(vs_path, fs_path, vs, fs);
	}

public:
	std::vector<uint32_t> vs_spirv;
	std::vector<uint32_t> fs_spirv;
	size_t vs_size = 0;
	size_t fs_size = 0;

	VKShader() = delete;

	VKShader(std::string vsPath, std::string fsPath){
		if(objectCnt++ == 0)
			SpirvHelper::Init();

		vs_path = vsPath;
		fs_path = fsPath;

		getVFShader(vs_spirv, fs_spirv);
		vs_size = vs_spirv.size() * sizeof(uint32_t);
		fs_size = fs_spirv.size() * sizeof(uint32_t);
	}

	~VKShader(){
		if(--objectCnt == 0)
			SpirvHelper::Finalize();
	}
};