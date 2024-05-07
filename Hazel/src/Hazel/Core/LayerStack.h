#pragma once

#include "Hazel/Core/Core.h"
#include "Layer.h"

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
