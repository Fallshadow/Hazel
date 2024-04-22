#pragma once

#include "Hazel/Renderer/VertexArray.h"
#include <vector>

namespace Hazel
{
	class OpenGLVertexArray : public VertexArray
	{
	public:
		OpenGLVertexArray();
		virtual ~OpenGLVertexArray();

		virtual void Bind() const override;
		virtual void UnBind() const override;

		virtual void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vb) override;
		virtual void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& ib) override;

		virtual const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffer() const override { return m_VertexBuffers; }
		virtual const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }
	private:
		uint32_t m_RendererID;
		std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
		std::shared_ptr<IndexBuffer> m_IndexBuffer;
	};
}