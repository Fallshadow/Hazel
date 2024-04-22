#pragma once

#include "hzpch.h"
#include "Hazel/Core.h"
#include "Hazel/Events/Event.h"

namespace Hazel
{
	// ������������
	struct WindowProps
	{
		std::string Title;
		unsigned int Width, Height;

		// ����Ĭ�ϲ���
		WindowProps(const std::string& title = "Hazel Engine", unsigned int width = 1280, unsigned int height = 720)
			: Title(title), Width(width), Height(height) { }
	};

	// �ӿ�
	// ��Ҫ��ÿ��ƽ̨��ʵ��
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
		
		// ��ӻ�ȡ����ڵĹ����������Ա��ڸ������͵Ĵ��ڻ�ȡ����
		virtual void* GetNativeWindow() const = 0;

		// ��������
		static Window* Create(const WindowProps& props = WindowProps());
	};

}