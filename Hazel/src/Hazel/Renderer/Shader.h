#pragma once

#include <string>

namespace Hazel
{
	class Shader
	{
	public:
		Shader(std::string& vertexStr, std::string& fragmentStr);
		~Shader();

		void Bind() const;
		void UnBind() const;
	private:
		uint32_t m_RendererID;
	};
}