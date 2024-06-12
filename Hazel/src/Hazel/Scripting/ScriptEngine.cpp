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
		// ���ļ����ص��ֽ�����
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

		// ����C#����
		static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath, bool loadPDB = false)
		{
			uint32_t fileSize = 0;
			char* fileData = ReadBytes(assemblyPath, &fileSize);

			// ע�⣺���ǲ��ܶ����ͼ��ִ�г��˼��س���������κβ�������Ϊ���ͼ��û�жԳ��򼯵�����
			MonoImageOpenStatus status;
			// ��������������Mono�����Ƿ�ϣ�����������ݣ��������Ǹ���洢�����������Ǵ���1����ʾMono�����ݸ��Ƶ��ڲ���������
			// ���ĸ�������ָ��MonoImageOpenStatusö�ٵ�ָ�룬���ǿ���ʹ�ô�ֵȷ��Mono�Ƿ��ܹ���ȡ�����ݣ������Ƿ�������
			// ���һ������Ҳ��һ������ֵ���������Ϊtrue��1����ʾMono���ԡ�����ģʽ���������ǵ�ͼ������ζ�����ǿ��Լ�����ͣ������������κδ��롣
			MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

			if (status != MONO_IMAGE_OK)
			{
				const char* errorMessage = mono_image_strerror(status);
				// ʹ�� errorMessage ���ݼ�¼һЩ������Ϣ
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

			// ͨ����Ч��ͼ����ش���һ��MonoAssembly
			// ����˺����ɹ������ǽ����ָ��MonoAssembly�ṹ��ָ�룬������������nullptr
			// ��һ�����������Ǵ�Mono��õ�ͼ�񣬵ڶ�������ʵ����ֻ��һ�����ƣ�Mono�����ڴ�ӡ����ʱʹ�ã�
			// ���������������ǵ�status�������˺������ڷ�������ʱд�����ǵ�status������������һ������Ĳ�Ӧ�����ɴ����������ǲ���������
			// ���һ��������mono_image_open_from_data_full�е����һ��������ͬ��������������ָ����1����Ӧ���ڴ˺�����Ҳ���������������ǵ�����£����ǽ�������Ϊ0��
			std::string pathString = assemblyPath.string();
			MonoAssembly* assembly = mono_assembly_load_from_full(image, pathString.c_str(), &status, 0);
			// ��ͼ������ڻ�ȡMonoAssemblyָ�룬��������
			mono_image_close(image);

			// ��Ҫ�����ͷ��ļ�����
			delete[] fileData;

			return assembly;
		}

		// ������ӡ�����е��������Ͷ���
		void PrintAssemblyTypes(MonoAssembly* assembly)
		{
			// ��ȡ����ͼ��
			MonoImage* image = mono_assembly_get_image(assembly);
			// ��ͼ���ȡ���Ͷ������Ϣ
			const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
			// �ӱ���Ϣ��ȡ���͵�������������
			int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

			for (int32_t i = 0; i < numTypes; i++)
			{
				// ��ǰ�е������ݣ������ж������ǵ����ݴ洢Ϊuint32_t���޷���32λ����
				// ������Ĵ�С����Ϊ�������ڵ����ı��������� MONO_TYPEDEF_SIZE
				uint32_t cols[MONO_TYPEDEF_SIZE];
				// ���ô˺��������ǵ� cols �������ڽ�����һ��ֵ���������ڿ���ʹ����Щֵ����ȡ�����͵�һЩ����
				mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

				// ��ͼ���л�ȡ�����ռ����������
				const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
				const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

				// ��ӡ�����ռ����������
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
		// mono��
		MonoDomain* RootDomain = nullptr;
		// C# ��
		MonoDomain* AppDomain = nullptr;

		// Hazel C# ����
		MonoAssembly* CoreAssembly = nullptr;
		MonoImage* CoreAssemblyImage = nullptr;

		// �û� C# ����
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

	// Ϊ���������ڴ沢�����޲ι���
	MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		MonoObject* instance = mono_object_new(s_SEData->AppDomain, monoClass);
		mono_runtime_object_init(instance);
		return instance;
	}

	// ����C#����
	void ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
	{
		// ����Ӧ�ó����򣬵�һ�������������Լ�������֣��ڶ��������������ļ�·�������ǲ���Ҫ
		s_SEData->AppDomain = mono_domain_create_appdomain("HazelScriptRuntime", nullptr);
		// ���µ�Ӧ�ó���������Ϊ��ǰӦ�ó����򣬵�һ������Ϊ�µ�Ӧ�ó����򣬵ڶ�������Ϊ�Ƿ�ǿ��ִ�У���ʵfalseӦ��Ҳ�У�true����������ж��Ӧ�ó�����ʱҲǿ������
		mono_domain_set(s_SEData->AppDomain, true);


		s_SEData->CoreAssemblyFilepath = filepath;
		// ����C#����
		s_SEData->CoreAssembly = Utils::LoadMonoAssembly(filepath, s_SEData->EnableDebugging);
		// ��ȡimage����
		s_SEData->CoreAssemblyImage = mono_assembly_get_image(s_SEData->CoreAssembly);
		// �鿴�����а����������ࡢ�ṹ���ö��
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
		// 1.����Entity����
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

			// 2.����Dll������C#��
			MonoClass* monoClass = mono_class_from_name(s_SEData->AppAssemblyImage, nameSpace, className);
			// entity���಻����
			if (monoClass == entityClass)
				continue;
			// 3.�жϵ�ǰ���Ƿ�ΪEntity������
			bool isEntity = mono_class_is_subclass_of(monoClass, entityClass, false);
			if (!isEntity)
				continue;
			// �����װ��Mono�����
			// 3.1�Ǿʹ���ű�map��
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
		// ��ȡ��ָ��
		m_MonoClass = mono_class_from_name(isCore ? s_SEData->CoreAssemblyImage : s_SEData->AppAssemblyImage, classNamespace.c_str(), className.c_str());
	}

	MonoObject* ScriptClass::Instantiate()
	{
		return ScriptEngine::InstantiateClass(m_MonoClass);
	}

	// ��ȡ����ָ��
	// �������һ������Ϊ�β�������������д-1���򽫷����ҵ��ĵ�һ������ָ�롣 ���غ���ָ�룬���û�ҵ������ؿ�ָ��
	// ��������ж���β�����һ�������ذ汾���˺����򲻹��ã���Ϊ������鷽����ʵ��ǩ��
	MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
	{
		return mono_class_get_method_from_name(m_MonoClass, name.c_str(), parameterCount);
	}

	// ���ú���
	MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
	{
		MonoObject* exception = nullptr;
		return mono_runtime_invoke(method, instance, params, &exception);
	}

	void ScriptEngine::InitMono()
	{
#pragma region ��ʼ��mono
		// ���ó���mscorlibĿ¼
		// ����ڵ�ǰ����Ŀ¼��·������ǰ��Hazelnut
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

		// ��ʼ��mono������һ����version�ĺ�����������һ����mono�Լ�ѡ��汾
		// �ڵ��ô˺���ʱ����ظ�������һ���ַ���������ַ��������ϴ���runtime������
		MonoDomain* rootDomain = mono_jit_init("HazelJITRuntime");
		HZ_CORE_ASSERT(rootDomain);

		// �ڵ��ô˺���ʱ�����ǻ�õ�һ�� MonoDomain ָ�룬��Ҫ��������Ҫ�洢���ָ�룬��Ϊ�Ժ����Ǳ����ֶ�������
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
