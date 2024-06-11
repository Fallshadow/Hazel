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
		static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath)
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
	}

	struct ScriptEngineData
	{
		// mono��
		MonoDomain* RootDomain = nullptr;
		// C# ��
		MonoDomain* AppDomain = nullptr;

		// C# ����
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

	// Ϊ���������ڴ沢�����޲ι���
	MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
		mono_runtime_object_init(instance);
		return instance;
	}

	// ����C#����
	void ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
	{
		// ����Ӧ�ó����򣬵�һ�������������Լ�������֣��ڶ��������������ļ�·�������ǲ���Ҫ
		s_Data->AppDomain = mono_domain_create_appdomain("HazelScriptRuntime", nullptr);
		// ���µ�Ӧ�ó���������Ϊ��ǰӦ�ó����򣬵�һ������Ϊ�µ�Ӧ�ó����򣬵ڶ�������Ϊ�Ƿ�ǿ��ִ�У���ʵfalseӦ��Ҳ�У�true����������ж��Ӧ�ó�����ʱҲǿ������
		mono_domain_set(s_Data->AppDomain, true);

		// ����C#����
		s_Data->CoreAssembly = Utils::LoadMonoAssembly(filepath);
		// ��ȡimage����
		s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
		// �鿴�����а����������ࡢ�ṹ���ö��
		// Utils::PrintAssemblyTypes(s_Data->CoreAssembly);
	}

	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className)
		: m_ClassNamespace(classNamespace), m_ClassName(className)
	{
		// ��ȡ��ָ��
		m_MonoClass = mono_class_from_name(s_Data->CoreAssemblyImage, classNamespace.c_str(), className.c_str());
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
		return mono_runtime_invoke(method, instance, params, nullptr);
	}

	void ScriptEngine::InitMono()
	{
#pragma region ��ʼ��mono
		// ���ó���mscorlibĿ¼
		// ����ڵ�ǰ����Ŀ¼��·������ǰ��Hazelnut
		mono_set_assemblies_path("mono/lib");

		// ��ʼ��mono������һ����version�ĺ�����������һ����mono�Լ�ѡ��汾
		// �ڵ��ô˺���ʱ����ظ�������һ���ַ���������ַ��������ϴ���runtime������
		MonoDomain* rootDomain = mono_jit_init("HazelJITRuntime");
		HZ_CORE_ASSERT(rootDomain);

		// �ڵ��ô˺���ʱ�����ǻ�õ�һ�� MonoDomain ָ�룬��Ҫ��������Ҫ�洢���ָ�룬��Ϊ�Ժ����Ǳ����ֶ�������
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
