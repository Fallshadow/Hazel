#include "hzpch.h"
#include "Shader.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include "Renderer.h"

namespace Hazel
{
	Shader* Shader::Create(const std::string& vertexStr, const std::string& fragmentStr)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:	HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:	return new OpenGLShader(vertexStr, fragmentStr);
		}

		HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}