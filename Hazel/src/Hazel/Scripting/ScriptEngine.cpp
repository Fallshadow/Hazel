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

		// Hazel C# 程序集
		MonoAssembly* CoreAssembly = nullptr;
		MonoImage* CoreAssemblyImage = nullptr;

		// 用户 C# 程序集
		MonoAssembly* AppAssembly = nullptr;
		MonoImage* AppAssemblyImage = nullptr;

		ScriptClass EntityClass;

		std::unordered_map<std::string, Ref<ScriptClass>> EntityClasses;
		std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;

		// Runtime
		Scene* SceneContext = nullptr;
	};

	static ScriptEngineData* s_SEData = nullptr;

	void ScriptEngine::Init()
	{
		s_SEData = new ScriptEngineData();
		InitMono();
		LoadAssembly("Resources/Scripts/Hazel-ScriptCore.dll");
		LoadAppAssembly("SandboxProject/Assets/Scripts/Binaries/Sandbox.dll");
		LoadAssemblyClasses();

		ScriptGlue::RegisterComponents();
		ScriptGlue::RegisterFunctions();

		s_SEData->EntityClass = ScriptClass("Hazel", "Entity", true);

#if 0
		MonoObject* instance = s_SEData->EntityClass.Instantiate();

		// Call method
		MonoMethod* printMessageFunc = s_SEData->EntityClass.GetMethod("PrintMessage", 0);
		s_SEData->EntityClass.InvokeMethod(instance, printMessageFunc);

		MonoMethod* printIntFunc = s_SEData->EntityClass.GetMethod("PrintInt", 1);
		int value = 5;
		void* param = &value;
		s_SEData->EntityClass.InvokeMethod(instance, printIntFunc, &param);

		MonoMethod* printIntsFunc = s_SEData->EntityClass.GetMethod("PrintInts", 2);
		int value2 = 508;
		void* params[2] =
		{
			&value,
			&value2
		};
		s_SEData->EntityClass.InvokeMethod(instance, printIntsFunc, params);

		MonoString* monoString = mono_string_new(s_SEData->AppDomain, "Hello World from C++!");
		MonoMethod* printCustomMessageFunc = s_SEData->EntityClass.GetMethod("PrintCustomMessage", 1);
		void* stringParam = monoString;
		s_SEData->EntityClass.InvokeMethod(instance, printCustomMessageFunc, &stringParam);
#endif
	}

	void ScriptEngine::Shutdown()
	{
		ShutdownMono();
		delete s_SEData;
	}

	// 为类分配对象内存并调用无参构造
	MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		MonoObject* instance = mono_object_new(s_SEData->AppDomain, monoClass);
		mono_runtime_object_init(instance);
		return instance;
	}

	// 加载C#程序集
	void ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
	{
		// 创建应用程序域，第一个参数是我们自己起的名字，第二个参数是配置文件路径，我们不需要
		s_SEData->AppDomain = mono_domain_create_appdomain("HazelScriptRuntime", nullptr);
		// 将新的应用程序域设置为当前应用程序域，第一个参数为新的应用程序域，第二个参数为是否强制执行，其实false应该也行，true可以让正在卸载应用程序域时也强行设置
		mono_domain_set(s_SEData->AppDomain, true);

		// 加载C#程序集
		s_SEData->CoreAssembly = Utils::LoadMonoAssembly(filepath);
		// 获取image引用
		s_SEData->CoreAssemblyImage = mono_assembly_get_image(s_SEData->CoreAssembly);
		// 查看程序集中包含的所有类、结构体和枚举
		Utils::PrintAssemblyTypes(s_SEData->CoreAssembly);
	}

	void ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
	{
		// Move this maybe
		s_SEData->AppAssembly = Utils::LoadMonoAssembly(filepath);

		s_SEData->AppAssemblyImage = mono_assembly_get_image(s_SEData->AppAssembly);

		Utils::PrintAssemblyTypes(s_SEData->AppAssembly);
	}

	void ScriptEngine::OnRuntimeStart(Scene* scene)
	{
		s_SEData->SceneContext = scene;
	}

	bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
	{
		return s_SEData->EntityClasses.find(fullClassName) != s_SEData->EntityClasses.end();
	}

	void ScriptEngine::OnCreateEntity(Entity entity)
	{
		const auto& sc = entity.GetComponent<ScriptComponent>();
		if (ScriptEngine::EntityClassExists(sc.ClassName))
		{
			Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(s_SEData->EntityClasses[sc.ClassName], entity);
			s_SEData->EntityInstances[entity.GetUUID()] = instance;
			instance->InvokeOnCreate();
		}
	}

	void ScriptEngine::OnUpdateEntity(Entity entity, Timestep ts)
	{
		UUID entityUUID = entity.GetUUID();
		HZ_CORE_ASSERT(s_SEData->EntityInstances.find(entityUUID) != s_SEData->EntityInstances.end());

		Ref<ScriptInstance> instance = s_SEData->EntityInstances[entityUUID];
		instance->InvokeOnUpdate((float)ts);
	}

	Scene* ScriptEngine::GetSceneContext()
	{
		return s_SEData->SceneContext;
	}

	void ScriptEngine::OnRuntimeStop()
	{
		s_SEData->SceneContext = nullptr;

		s_SEData->EntityInstances.clear();
	}

	std::unordered_map<std::string, Ref<ScriptClass>> ScriptEngine::GetEntityClasses()
	{
		return s_SEData->EntityClasses;
	}

	void ScriptEngine::LoadAssemblyClasses()
	{
		s_SEData->EntityClasses.clear();

		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(s_SEData->AppAssemblyImage, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);
		// 1.加载Entity父类
		MonoClass* entityClass = mono_class_from_name(s_SEData->CoreAssemblyImage, "Hazel", "Entity");

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(s_SEData->AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(s_SEData->AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);
			std::string fullName;
			if (strlen(nameSpace) != 0)
				fullName = fmt::format("{}.{}", nameSpace, name);
			else
				fullName = name;

			// 2.加载Dll中所有C#类
			MonoClass* monoClass = mono_class_from_name(s_SEData->AppAssemblyImage, nameSpace, name);
			// entity父类不保存
			if (monoClass == entityClass)
				continue;
			// 3.判断当前类是否为Entity的子类
			bool isEntity = mono_class_is_subclass_of(monoClass, entityClass, false);
			if (isEntity)
				// 存入封装的Mono类对象
				// 3.1是就存入脚本map中
				s_SEData->EntityClasses[fullName] = CreateRef<ScriptClass>(nameSpace, name);
		}
	}

	MonoImage* ScriptEngine::GetCoreAssemblyImage()
	{
		return s_SEData->CoreAssemblyImage;
	}

	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore)
		: m_ClassNamespace(classNamespace), m_ClassName(className)
	{
		// 获取类指针
		m_MonoClass = mono_class_from_name(isCore ? s_SEData->CoreAssemblyImage : s_SEData->AppAssemblyImage, classNamespace.c_str(), className.c_str());
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
		s_SEData->RootDomain = rootDomain;
#pragma endregion
	}

	void ScriptEngine::ShutdownMono()
	{
		// NOTE(Yan): mono is a little confusing to shutdown, so maybe come back to this

		// mono_domain_unload(s_SEData->AppDomain);
		s_SEData->AppDomain = nullptr;

		// mono_jit_cleanup(s_SEData->RootDomain);
		s_SEData->RootDomain = nullptr;
	}

	ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
		: m_ScriptClass(scriptClass)
	{
		m_Instance = scriptClass->Instantiate();

		m_Constructor = s_SEData->EntityClass.GetMethod(".ctor", 1);
		m_OnCreateMethod = scriptClass->GetMethod("OnCreate", 0);
		m_OnUpdateMethod = scriptClass->GetMethod("OnUpdate", 1);

		// Call Entity constructor
		{
			UUID entityID = entity.GetUUID();
			void* param = &entityID;
			m_ScriptClass->InvokeMethod(m_Instance, m_Constructor, &param);
		}
	}

	void ScriptInstance::InvokeOnCreate()
	{
		if (m_OnCreateMethod)
			m_ScriptClass->InvokeMethod(m_Instance, m_OnCreateMethod);
	}

	void ScriptInstance::InvokeOnUpdate(float ts)
	{
		if (m_OnUpdateMethod)
		{
			void* param = &ts;
			m_ScriptClass->InvokeMethod(m_Instance, m_OnUpdateMethod, &param);
		}
	}
}
