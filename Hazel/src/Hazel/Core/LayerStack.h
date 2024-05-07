#pragma once

#include "Hazel/Core/Core.h"
#include "Layer.h"

#include <vector>

namespace Hazel
{
	// ��İ�װ
	// ���Ϊ��ͨ��͸��ǲ�
	// ��ͨ��������Ϊ����ջ�����ǲ�����λ����ͨ��ջ���µ���һ��ջ
	// �����ϰ�����������һ��m_LayerInsert��ʼ��λ��ջ������ֻ����ͨջ��Ӱ����
	class LayerStack
	{
	public:
		LayerStack();
		~LayerStack();

		void PushLay(Layer* lay);
		void PushOverlay(Layer* overlay);
		void PopLay(Layer* lay);
		void PopOverlay(Layer* overlay);

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }
	private:
		std::vector<Layer*> m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	};
}
