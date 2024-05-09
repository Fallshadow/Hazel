#pragma once

#include "Hazel/Core/Core.h"
#include "Hazel/Core/Layer.h"

#include <vector>

namespace Hazel
{
	// 层的包装
	// 层分为普通层和覆盖层
	// 普通层可以理解为正常栈，覆盖层则是位于普通层栈底下的另一个栈
	// 代码上把这两个放在一起，m_LayerInsert则始终位于栈顶，且只有普通栈能影响它
	class LayerStack
	{
	public:
		LayerStack() = default;
		~LayerStack();

		void PushLay(Layer* lay);
		void PushOverlay(Layer* overlay);
		void PopLay(Layer* lay);
		void PopOverlay(Layer* overlay);

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }

		std::vector<Layer*>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
		std::vector<Layer*>::reverse_iterator rend() { return m_Layers.rend(); }

		std::vector<Layer*>::const_iterator begin() const { return m_Layers.begin(); }
		std::vector<Layer*>::const_iterator end()	const { return m_Layers.end(); }
		std::vector<Layer*>::const_reverse_iterator rbegin() const { return m_Layers.rbegin(); }
		std::vector<Layer*>::const_reverse_iterator rend() const { return m_Layers.rend(); }
	private:
		std::vector<Layer*> m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	};
}
