#pragma once

#include "hzpch.h"
#include "Hazel/Core/Core.h"

namespace Hazel
{
	// �¼�ϵͳ�����������������͡���ζ�ŵ�һ���¼�����ʱ���ͻ�����ϵͳ��������������ֱ�������꣬�ٷ��ء�
	// δ�������õĲ����ǽ��¼����浽�¼��أ�Ȼ����Update�ġ��¼�����һſȥ���д���

	// �¼�
	enum class EventType
	{
		None = 0,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
		AppTick, AppUpdate, AppRender,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
	};

	// �¼����� ��λ
	enum EventCategory
	{
		None = 0,
		EventCategoryApplication = BIT(0),
		EventCategoryInput = BIT(1),
		EventCategoryKeyboard = BIT(2),
		EventCategoryMouse = BIT(3),
		EventCategoryMouseButton = BIT(4),
	};

// ���ú궨�� �����¼�������غ������¼���������
// ��ȡ��̬�¼����͡���ȡ�¼�ö�����ͣ�����д������ȡ�¼����ƣ�����д��
#define EVENT_CLASS_TYPE(type) static EventType GetStaticType(){ return EventType::type; }\
virtual EventType GetEventType() const override { return GetStaticType();}\
virtual const char* GetName() const override {return #type;}

// ���ú궨�� �����¼�����������غ������¼������������� 
#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

	// �¼�����
	class HAZEL_API Event
	{
	public:
		bool Handled = false;

		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		// ֻ���ڵ��ԣ����Բ�̫��ע���ܣ��ǲ���Ӧ���ú��������
		virtual std::string ToString() const { return GetName(); }

		inline bool IsInCategory(EventCategory category)
		{
			return GetCategoryFlags() & category;
		}
	protected:

	};

	class EventDispatcher
	{
	public:
		EventDispatcher(Event& event) :m_Event(event)
		{
		}

		// TODO:�����﷨
		template<typename T, typename F>
		bool Dispatch(const F& func)
		{
			if (m_Event.GetEventType() == T::GetStaticType())
			{
				m_Event.Handled = func(static_cast<T&>(m_Event));
				return true;
			}

			return false;
		}
	private:
		Event& m_Event;
	};

	// �������Event
	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}
}