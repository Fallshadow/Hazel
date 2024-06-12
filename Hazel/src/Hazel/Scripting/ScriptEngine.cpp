#include "hzpch.h"
#include "ScriptEngine.h"

#include "ScriptGlue.h"

#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/object.h"
#include "mono/metadata/tabledefs.h"
#include "mono/metadata/mono-debug.h"
#include "mono/metadata/threads.h"

#include "FileWatch.h"

#include "Hazel/Core/Application.h"
#include "Hazel/Core/Timer.h"

namespace Hazel 
{
	static std::unordered_map<std::string, ScriptFieldType> s_ScriptFieldTypeMap =
	{
		{ "System.Single"	, ScriptFieldType::Float },
		{ "System.Double"	, ScriptFieldType::Double },
		{ "System.Boolean"	, ScriptFieldType::Bool },
		{ "System.Char"		, ScriptFieldType::Char },
		{ "System.Int16"	, ScriptFieldType::Short },
		{ "System.Int32"	, ScriptFieldType::Int },
		{ "System.Int64"	, ScriptFieldType::Long },
		{ "System.Byte"		, ScriptFieldType::Byte },
		{ "System.UInt16"	, ScriptFieldType::UShort },
		{ "System.UInt32"	, ScriptFieldType::UInt },
		{ "System.UInt64"	, ScriptFieldType::ULong },

		{ "Hazel.Vector2"	, ScriptFieldType::Vector2 },
		{ "Hazel.Vector3"	, ScriptFieldType::Vector3 },
		{ "Hazel.Vector4"	, ScriptFieldType::Vector4 },

		{ "Hazel.Entity", ScriptFieldType::Entity },
	};

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
		static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath, bool loadPDB = false)
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

			if (loadPDB)
			{
				std::filesystem::path pdbPath = assemblyPath;
				pdbPath.replace_extension(".pdb");

				if (std::filesystem::exists(pdbPath))
				{
					uint32_t pdbFileSize = 0;
					char* pdbFileData = ReadBytes(pdbPath, &pdbFileSize);
					mono_debug_open_image_from_memory(image, (const mono_byte*)pdbFileData, pdbFileSize);
					HZ_CORE_INFO("Loaded PDB {}", pdbPath);
					delete[] pdbFileData;
				}
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

		ScriptFieldType MonoTypeToScriptFieldType(MonoType* monoType)
		{
			std::string typeName = mono_type_get_name(monoType);

			auto it = s_ScriptFieldTypeMap.find(typeName);
			if (it == s_ScriptFieldTypeMap.end())
			{
				HZ_CORE_ERROR("Unknown type: {}", typeName);
				return ScriptFieldType::None;
			}

			return it->second;
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

		std::filesystem::path CoreAssemblyFilepath;
		std::filesystem::path AppAssemblyFilepath;

		ScriptClass EntityClass;

		std::unordered_map<std::string, Ref<ScriptClass>> EntityClasses;
		std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;
		std::unordered_map<UUID, ScriptFieldMap> EntityScriptFields;

		Scope<filewatch::FileWatch<std::string>> AppAssemblyFileWatcher;
		bool AssemblyReloadPending = false;

		bool EnableDebugging = true;

		// Runtime
		Scene* SceneContext = nullptr;
	};

	static ScriptEngineData* s_SEData = nullptr;

	static void OnAppAssemblyFileSystemEvent(const std::string& path, const filewatch::Event change_type)
	{
		if (!s_SEData->AssemblyReloadPending && change_type == filewatch::Event::modified)
		{
			s_SEData->AssemblyReloadPending = true;

			Application::Get().SubmitToMainThread([]()
				{
					s_SEData->AppAssemblyFileWatcher.reset();
					ScriptEngine::ReloadAssembly();
				});
		}
	}

	void ScriptEngine::Init()
	{
		s_SEData = new ScriptEngineData();
		InitMono();
		ScriptGlue::RegisterFunctions();

		LoadAssembly("Resources/Scripts/Hazel-ScriptCore.dll");
		LoadAppAssembly("SandboxProject/Assets/Scripts/Binaries/Sandbox.dll");
		LoadAssemblyClasses();

		ScriptGlue::RegisterComponents();

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

	MonoObject* ScriptEngine::GetManagedInstance(UUID uuid)
	{
		HZ_CORE_ASSERT(s_SEData->EntityInstances.find(uuid) != s_SEData->EntityInstances.end());
		return s_SEData->EntityInstances.at(uuid)->GetManagedObject();
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


		s_SEData->CoreAssemblyFilepath = filepath;
		// 加载C#程序集
		s_SEData->CoreAssembly = Utils::LoadMonoAssembly(filepath, s_SEData->EnableDebugging);
		// 获取image引用
		s_SEData->CoreAssemblyImage = mono_assembly_get_image(s_SEData->CoreAssembly);
		// 查看程序集中包含的所有类、结构体和枚举
		Utils::PrintAssemblyTypes(s_SEData->CoreAssembly);
	}

	void ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
	{
		// Move this maybe
		s_SEData->AppAssemblyFilepath = filepath;
		s_SEData->AppAssembly = Utils::LoadMonoAssembly(filepath, s_SEData->EnableDebugging);

		s_SEData->AppAssemblyImage = mono_assembly_get_image(s_SEData->AppAssembly);

		Utils::PrintAssemblyTypes(s_SEData->AppAssembly);

		s_SEData->AppAssemblyFileWatcher = CreateScope<filewatch::FileWatch<std::string>>(filepath.string(), OnAppAssemblyFileSystemEvent);
		s_SEData->AssemblyReloadPending = false;
	}

	void ScriptEngine::ReloadAssembly()
	{
		mono_domain_set(mono_get_root_domain(), false);

		mono_domain_unload(s_SEData->AppDomain);

		LoadAssembly(s_SEData->CoreAssemblyFilepath);
		LoadAppAssembly(s_SEData->AppAssemblyFilepath);
		LoadAssemblyClasses();

		ScriptGlue::RegisterComponents();

		// Retrieve and instantiate class
		s_SEData->EntityClass = ScriptClass("Hazel", "Entity", true);
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
			UUID entityID = entity.GetUUID();

			Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(s_SEData->EntityClasses[sc.ClassName], entity);
			s_SEData->EntityInstances[entityID] = instance;

			// Copy field values
			if (s_SEData->EntityScriptFields.find(entityID) != s_SEData->EntityScriptFields.end())
			{
				const ScriptFieldMap& fieldMap = s_SEData->EntityScriptFields.at(entityID);
				for (const auto& [name, fieldInstance] : fieldMap)
					instance->SetFieldValueInternal(name, fieldInstance.m_Buffer);
			}

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

	Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID entityID)
	{
		auto it = s_SEData->EntityInstances.find(entityID);
		if (it == s_SEData->EntityInstances.end())
			return nullptr;

		return it->second;
	}

	Ref<ScriptClass> ScriptEngine::GetEntityClass(const std::string& name)
	{
		if (s_SEData->EntityClasses.find(name) == s_SEData->EntityClasses.end())
			return nullptr;

		return s_SEData->EntityClasses.at(name);
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

	ScriptFieldMap& ScriptEngine::GetScriptFieldMap(Entity entity)
	{
		HZ_CORE_ASSERT(entity);

		UUID entityID = entity.GetUUID();
		return s_SEData->EntityScriptFields[entityID];
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
			const char* className = mono_metadata_string_heap(s_SEData->AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);
			std::string fullName;
			if (strlen(nameSpace) != 0)
				fullName = fmt::format("{}.{}", nameSpace, className);
			else
				fullName = className;

			// 2.加载Dll中所有C#类
			MonoClass* monoClass = mono_class_from_name(s_SEData->AppAssemblyImage, nameSpace, className);
			// entity父类不保存
			if (monoClass == entityClass)
				continue;
			// 3.判断当前类是否为Entity的子类
			bool isEntity = mono_class_is_subclass_of(monoClass, entityClass, false);
			if (!isEntity)
				continue;
			// 存入封装的Mono类对象
			// 3.1是就存入脚本map中
			Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(nameSpace, className);
			s_SEData->EntityClasses[fullName] = scriptClass;


			// This routine is an iterator routine for retrieving the fields in a class.
			// You must pass a gpointer that points to zero and is treated as an opaque handle
			// to iterate over all of the elements. When no more values are available, the return value is NULL.

			int fieldCount = mono_class_num_fields(monoClass);
			HZ_CORE_WARN("{} has {} fields:", className, fieldCount);
			void* iterator = nullptr;
			while (MonoClassField* field = mono_class_get_fields(monoClass, &iterator))
			{
				const char* fieldName = mono_field_get_name(field);
				uint32_t flags = mono_field_get_flags(field);
				if (flags & FIELD_ATTRIBUTE_PUBLIC)
				{
					MonoType* type = mono_field_get_type(field);
					ScriptFieldType fieldType = Utils::MonoTypeToScriptFieldType(type);
					HZ_CORE_WARN("  {} ({})", fieldName, Utils::ScriptFieldTypeToString(fieldType));

					scriptClass->m_Fields[fieldName] = { fieldType, fieldName, field };
				}
			}
		}

		auto& entityClasses = s_SEData->EntityClasses;

		//mono_field_get_value()
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
		MonoObject* exception = nullptr;
		return mono_runtime_invoke(method, instance, params, &exception);
	}

	void ScriptEngine::InitMono()
	{
#pragma region 初始化mono
		// 设置程序集mscorlib目录
		// 相对于当前工作目录的路径，当前在Hazelnut
		mono_set_assemblies_path("mono/lib");

		if (s_SEData->EnableDebugging)
		{
			const char* argv[2] = {
				"--debugger-agent=transport=dt_socket,address=127.0.0.1:2550,server=y,suspend=n,loglevel=3,logfile=MonoDebugger.log",
				"--soft-breakpoints"
			};

			mono_jit_parse_options(2, (char**)argv);
			mono_debug_init(MONO_DEBUG_FORMAT_MONO);
		}

		// 初始化mono，还有一个带version的函数，但我们一般让mono自己选择版本
		// 在调用此函数时，务必给它传递一个字符串，这个字符串本质上代表runtime的名称
		MonoDomain* rootDomain = mono_jit_init("HazelJITRuntime");
		HZ_CORE_ASSERT(rootDomain);

		// 在调用此函数时，我们会得到一个 MonoDomain 指针，重要的是我们要存储这个指针，因为稍后我们必须手动清理它
		s_SEData->RootDomain = rootDomain;

		if (s_SEData->EnableDebugging)
			mono_debug_domain_create(s_SEData->RootDomain);

		mono_thread_set_main(mono_thread_current());
#pragma endregion
	}

	void ScriptEngine::ShutdownMono()
	{
		mono_domain_set(mono_get_root_domain(), false);

		mono_domain_unload(s_SEData->AppDomain);
		s_SEData->AppDomain = nullptr;

		mono_jit_cleanup(s_SEData->RootDomain);
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

	bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* buffer)
	{
		const auto& fields = m_ScriptClass->GetFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return false;

		const ScriptField& field = it->second;
		mono_field_get_value(m_Instance, field.ClassField, buffer);
		return true;
	}

	bool ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value)
	{
		const auto& fields = m_ScriptClass->GetFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return false;

		const ScriptField& field = it->second;
		mono_field_set_value(m_Instance, field.ClassField, (void*)value);
		return true;
	}
}
