#include "hzpch.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Hazel/Renderer/RenderCommand.h"

namespace Hazel
{
	Scope<RendererAPI> RenderCommand::s_RendererAPI = CreateScope<OpenGLRendererAPI>();
}