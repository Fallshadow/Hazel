#pragma once

#include "hzpch.h"
#include "Hazel/Core/Base.h"
#include "Hazel/Events/Event.h"

namespace Hazel
{
	// 窗口属性数据
	struct WindowProps
	{
		std::string Title;
		uint32_t Width, Height;

		// 窗口默认参数
		WindowProps(const std::string& title = "Hazel Engine", 
				uint32_t width = 1280, 
				uint32_t height = 720)
			: Title(title), Width(width), Height(height) { }
	};

	// 抽象类 需要在每个平台上实现
	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() = default;
		virtual void OnUpdate() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual void* GetNativeWindow() const = 0;

		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

		static Scope<Window> Create(const WindowProps& props = WindowProps());
	};
}