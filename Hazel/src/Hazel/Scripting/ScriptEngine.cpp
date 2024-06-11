#include "hzpch.h"
#include "ScriptEngine.h"

#include "ScriptGlue.h"

#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/object.h"


namespace Hazel 
{
	namespace Utils
	{
		// 将文件加载到字节数组
		static char* ReadBytes(const std::filesystem::path& filepath, uint32_t* outSize)
		{
			std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

			if (!stream)
			{
				// Failed to open the file
				return nullptr;
			}

			std::streampos end = stream.tellg();
			stream.seekg(0, std::ios::beg);
			uint64_t size = end - stream.tellg();

			if (size == 0)
			{
				// File is empty
				return nullptr;
			}

			char* buffer = new char[size];
			stream.read((char*)buffer, size);
			stream.close();

			*outSize = (uint32_t)size;
			return buffer;
		}

		// 加载C#程序集
		static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath)
		{
			uint32_t fileSize = 0;
			char* fileData = ReadBytes(assemblyPath, &fileSize);

			// 注意：我们不能对这个图像执行除了加载程序集以外的任何操作，因为这个图像没有对程序集的引用
			MonoImageOpenStatus status;
			// 第三个参数告诉Mono我们是否希望它复制数据，还是我们负责存储它，这里我们传递1，表示Mono将数据复制到内部缓冲区中
			// 第四个参数是指向MonoImageOpenStatus枚举的指针，我们可以使用此值确定Mono是否能够读取该数据，或者是否有问题
			// 最后一个参数也是一个布尔值，如果设置为true或1，表示Mono将以“反射模式”加载我们的图像，这意味着我们可以检查类型，但不能运行任何代码。
			MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

			if (status != MONO_IMAGE_OK)
			{
				const char* errorMessage = mono_image_strerror(status);
				// 使用 errorMessage 数据记录一些错误消息
				return nullptr;
			}

			// 通过有效的图像加载创建一个MonoAssembly
			// 如果此函数成功，我们将获得指向MonoAssembly结构的指针，否则它将返回nullptr
			// 第一个参数是我们从Mono获得的图像，第二个参数实际上只是一个名称，Mono可以在打印错误时使用，
			// 第三个参数是我们的status变量，此函数将在发生错误时写入我们的status变量，但在这一点上真的不应该生成错误，所以我们不会检查它。
			// 最后一个参数与mono_image_open_from_data_full中的最后一个参数相同，因此如果在那里指定了1，你应该在此函数中也这样做，但在我们的情况下，我们将其设置为0。
			std::string pathString = assemblyPath.string();
			MonoAssembly* assembly = mono_assembly_load_from_full(image, pathString.c_str(), &status, 0);
			// 该图像仅用于获取MonoAssembly指针，现已无用
			mono_image_close(image);

			// 不要忘记释放文件数据
			delete[] fileData;

			return assembly;
		}

		// 迭代打印程序集中的所有类型定义
		void PrintAssemblyTypes(MonoAssembly* assembly)
		{
			// 获取程序集图像
			MonoImage* image = mono_assembly_get_image(assembly);
			// 从图像获取类型定义表信息
			const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
			// 从表信息获取类型的数量，即行数
			int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

			for (int32_t i = 0; i < numTypes; i++)
			{
				// 当前行的列数据，所有列都将它们的数据存储为uint32_t即无符号32位整数
				// 将数组的大小设置为我们正在迭代的表的最大列数 MONO_TYPEDEF_SIZE
				uint32_t cols[MONO_TYPEDEF_SIZE];
				// 调用此函数后，我们的 cols 数组现在将包含一堆值，我们现在可以使用这些值来获取此类型的一些数据
				mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

				// 从图像中获取命名空间和类型名称
				const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
				const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

				// 打印命名空间和类型名称
				HZ_CORE_TRACE("{}.{}", nameSpace, name);
			}
		}
	}

	struct ScriptEngineData
	{
		// mono域
		MonoDomain* RootDomain = nullptr;
		// C# 域
		MonoDomain* AppDomain = nullptr;

		// C# 程序集
		MonoAssembly* CoreAssembly = nullptr;
		MonoImage* CoreAssemblyImage = nullptr;

		ScriptClass EntityClass;
	};

	static ScriptEngineData* s_Data = nullptr;

	void ScriptEngine::Init()
	{
		s_Data = new ScriptEngineData();
		InitMono();
		LoadAssembly("Resources/Scripts/Hazel-ScriptCore.dll");
		ScriptGlue::RegisterFunctions();

		s_Data->EntityClass = ScriptClass("Hazel", "Entity");

		MonoObject* instance = s_Data->EntityClass.Instantiate();

		// Call method
		MonoMethod* printMessageFunc = s_Data->EntityClass.GetMethod("PrintMessage", 0);
		s_Data->EntityClass.InvokeMethod(instance, printMessageFunc);

		MonoMethod* printIntFunc = s_Data->EntityClass.GetMethod("PrintInt", 1);
		int value = 5;
		void* param = &value;
		s_Data->EntityClass.InvokeMethod(instance, printIntFunc, &param);

		MonoMethod* printIntsFunc = s_Data->EntityClass.GetMethod("PrintInts", 2);
		int value2 = 508;
		void* params[2] =
		{
			&value,
			&value2
		};
		s_Data->EntityClass.InvokeMethod(instance, printIntsFunc, params);

		MonoString* monoString = mono_string_new(s_Data->AppDomain, "Hello World from C++!");
		MonoMethod* printCustomMessageFunc = s_Data->EntityClass.GetMethod("PrintCustomMessage", 1);
		void* stringParam = monoString;
		s_Data->EntityClass.InvokeMethod(instance, printCustomMessageFunc, &stringParam);

		HZ_CORE_ASSERT(false);
	}

	void ScriptEngine::Shutdown()
	{
		ShutdownMono();
		delete s_Data;
	}

	// 为类分配对象内存并调用无参构造
	MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
		mono_runtime_object_init(instance);
		return instance;
	}

	// 加载C#程序集
	void ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
	{
		// 创建应用程序域，第一个参数是我们自己起的名字，第二个参数是配置文件路径，我们不需要
		s_Data->AppDomain = mono_domain_create_appdomain("HazelScriptRuntime", nullptr);
		// 将新的应用程序域设置为当前应用程序域，第一个参数为新的应用程序域，第二个参数为是否强制执行，其实false应该也行，true可以让正在卸载应用程序域时也强行设置
		mono_domain_set(s_Data->AppDomain, true);

		// 加载C#程序集
		s_Data->CoreAssembly = Utils::LoadMonoAssembly(filepath);
		// 获取image引用
		s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
		// 查看程序集中包含的所有类、结构体和枚举
		// Utils::PrintAssemblyTypes(s_Data->CoreAssembly);
	}

	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className)
		: m_ClassNamespace(classNamespace), m_ClassName(className)
	{
		// 获取类指针
		m_MonoClass = mono_class_from_name(s_Data->CoreAssemblyImage, classNamespace.c_str(), className.c_str());
	}

	MonoObject* ScriptClass::Instantiate()
	{
		return ScriptEngine::InstantiateClass(m_MonoClass);
	}

	// 获取函数指针
	// 其中最后一个参数为形参数量，可以填写-1，则将返回找到的第一个函数指针。 返回函数指针，如果没找到，返回空指针
	// 如果函数有多个形参数量一样的重载版本，此函数则不管用，因为它不检查方法的实际签名
	MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
	{
		return mono_class_get_method_from_name(m_MonoClass, name.c_str(), parameterCount);
	}

	// 调用函数
	MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
	{
		return mono_runtime_invoke(method, instance, params, nullptr);
	}

	void ScriptEngine::InitMono()
	{
#pragma region 初始化mono
		// 设置程序集mscorlib目录
		// 相对于当前工作目录的路径，当前在Hazelnut
		mono_set_assemblies_path("mono/lib");

		// 初始化mono，还有一个带version的函数，但我们一般让mono自己选择版本
		// 在调用此函数时，务必给它传递一个字符串，这个字符串本质上代表runtime的名称
		MonoDomain* rootDomain = mono_jit_init("HazelJITRuntime");
		HZ_CORE_ASSERT(rootDomain);

		// 在调用此函数时，我们会得到一个 MonoDomain 指针，重要的是我们要存储这个指针，因为稍后我们必须手动清理它
		s_Data->RootDomain = rootDomain;
#pragma endregion
	}

	void ScriptEngine::ShutdownMono()
	{
		// NOTE(Yan): mono is a little confusing to shutdown, so maybe come back to this

		// mono_domain_unload(s_Data->AppDomain);
		s_Data->AppDomain = nullptr;

		// mono_jit_cleanup(s_Data->RootDomain);
		s_Data->RootDomain = nullptr;
	}

}
