#include "hzpch.h"
#include "Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Hazel
{
	Shader::Shader(std::string& vertexStr, std::string& fragmentStr)
	{
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		const GLchar* source = vertexStr.c_str();
		glShaderSource(vertexShader, 1, &source, 0);
		glCompileShader(vertexShader);

		GLint isCompiled = 0;
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(vertexShader, maxLength, &maxLength, &infoLog[0]);

			glDeleteShader(vertexShader);

			HZ_CORE_ERROR("{0}", infoLog.data());
			HZ_CORE_ASSERT(false, "Vertex shader compilation failure!");
			return;
		}

		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		source = fragmentStr.c_str();
		glShaderSource(fragmentShader, 1, &source, 0);
		glCompileShader(fragmentShader);

		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &infoLog[0]);

			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);

			HZ_CORE_ERROR("{0}", infoLog.data());
			HZ_CORE_ASSERT(false, "Fragment shader compilation failure!");
			return;
		}

		m_RendererID = glCreateProgram();
		GLuint program = m_RendererID;

		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

			glDeleteProgram(program);
			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);


			HZ_CORE_ERROR("{0}", infoLog.data());
			HZ_CORE_ASSERT(false, "Shader link failure!");
			return;
		}

		glDetachShader(program, vertexShader);
		glDetachShader(program, fragmentShader);
	}

	Shader::~Shader()
	{
		glDeleteProgram(m_RendererID);
	}

	void Shader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void Shader::UnBind() const
	{
		glUseProgram(0);
	}

	void Shader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}
}