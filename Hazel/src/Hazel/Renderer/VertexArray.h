#pragma once

#include <memory>
#include "Buffer.h"

namespace Hazel
{

	class VertexArray
	{
	public:
		virtual ~VertexArray() = default;
		virtual void Bind() const = 0;
		virtual void UnBind() const = 0;

		virtual void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vb) = 0;
		virtual void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& ib) = 0;

		virtual const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffer() const = 0;
		virtual const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const = 0;

		static VertexArray* Create();
	};
}