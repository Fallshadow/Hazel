#include "hzpch.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Hazel
{
	Renderer::SceneData* Renderer::s_SceneData = new Renderer::SceneData;

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		s_SceneData->ViewProjectionMatrix = camera.GetViewProjectMatrix();
	}

	void Renderer::EndScene()
	{
	}

	void Renderer::Submit(
		const std::shared_ptr<Shader>& shader, 
		const std::shared_ptr<VertexArray>& vertexArray, 
		const glm::mat4& transform)
	{
		shader->Bind();
		// 其实这些方法也应该变成抽象方法，而不是进行转换
		std::dynamic_pointer_cast<OpenGLShader>(shader)->UploadUniformMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
		std::dynamic_pointer_cast<OpenGLShader>(shader)->UploadUniformMat4("u_Transform", transform);

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
}