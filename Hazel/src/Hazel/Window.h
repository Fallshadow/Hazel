#pragma once

#include "hzpch.h"
#include "Hazel/Core.h"
#include "Hazel/Events/Event.h"

namespace Hazel
{
	// 窗口属性数据
	struct WindowProps
	{
		std::string Title;
		unsigned int Width, Height;

		// 窗口默认参数
		WindowProps(const std::string& title = "Hazel Engine", unsigned int width = 1280, unsigned int height = 720)
			: Title(title), Width(width), Height(height) { }
	};

	// 接口
	// 需要在每个平台上实现
	class HAZEL_API Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() = default;
		virtual void OnUpdate() = 0;
		virtual unsigned int GetWidth() const = 0;
		virtual unsigned int GetHeight() const = 0;

		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;
		
		// 添加获取活动窗口的公开函数，以便于各种类型的窗口获取需求
		virtual void* GetNativeWindow() const = 0;

		// 创建窗口
		static Window* Create(const WindowProps& props = WindowProps());
	};

}