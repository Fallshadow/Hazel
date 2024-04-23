#pragma once

#include <string>
#include <glm/glm.hpp>

namespace Hazel
{
	class Shader
	{
	public:
		Shader(std::string& vertexStr, std::string& fragmentStr);
		~Shader();

		void Bind() const;
		void UnBind() const;

		void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);
	private:
		uint32_t m_RendererID;
	};
}