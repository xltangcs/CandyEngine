#include "CandyPCH.h"
#include "Candy/Scripting/ScriptSystem.h"

#include "Candy/Scripting/ScriptObject.h"
#include "Candy/Scene/Entity.h"
#include "Candy/Scene/Components.h"
#include "Candy/Core/Timestep.h"
#include "Candy/Core/UUID.h"
#include "Candy/Core/FileSystem.h"
#include "Candy/Project/ProjectUtils.h"

#include <pybind11/embed.h>

#include <filesystem>
#include <fstream>
#include <windows.h>

namespace py = pybind11;

extern void PyBindings_ForceLink();

namespace Candy {
    
struct ScriptInstance
{
    py::object PyObject;
    ScriptObject* Script = nullptr;
    Entity StoredEntity;
};

ScriptSystem& ScriptSystem::Get()
{
    static ScriptSystem s_Instance;
    return s_Instance;
}

void ScriptSystem::InitPython()
{
    if (m_PythonInitialized)
        return;

    ::PyBindings_ForceLink();  // Ensure PYBIND11_EMBEDDED_MODULE gets linked

    // Resolve exe directory
    std::filesystem::path exePath = []() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf);
    }();
    std::filesystem::path exeDir = exePath.parent_path();

    // Embedded Python: python314.dll, python314.zip, python314._pth next to exe
    std::wstring pythonHome = exeDir.wstring();
    Py_SetPythonHome(pythonHome.c_str());

    Py_Initialize();

    // Resolve scripts directory: project Content/Scripts (filesystem path)
    std::filesystem::path scriptsDir = std::filesystem::absolute(ProjectUtils::GetProjectContentPath() / "Scripts");
    if (std::filesystem::exists(scriptsDir))
    {
        py::module_ sys = py::module_::import("sys");
        sys.attr("path").attr("append")(scriptsDir.string());
    }

    m_PythonInitialized = true;
}

void ScriptSystem::ShutdownPython()
{
    if (!m_PythonInitialized)
        return;

    for (auto& [id, instance] : m_Instances)
    {
        if (instance->Script)
            instance->Script->OnDestroy();
    }
    m_Instances.clear();

    // NOTE: Intentionally do NOT call Py_Finalize(). The interpreter is process-global
    // and reclaimed by the OS on exit. Finalizing it while pybind11's embedded module
    // ("candy") still has live static type info causes an access violation.
}

static std::filesystem::path ResolveScriptPath(const std::string& scriptPath)
{
    std::filesystem::path p(scriptPath);
    if (p.is_absolute())
        return p;

    return std::filesystem::absolute(ProjectUtils::GetProjectContentPath() / p);
}

void ScriptSystem::InstantiateScript(Entity& entity)
{
    UUID uuid = entity.GetUUID();
    auto& sc = entity.GetComponent<ScriptComponent>();

    if (m_Instances.find(uuid) != m_Instances.end())
    {
        CANDY_CORE_WARN("Script already instantiated for entity {0}", (uint64_t)uuid);
        return;
    }

    try
    {
        // Resolve script file to absolute path
        std::filesystem::path scriptFullPath = ResolveScriptPath(sc.ScriptPath);

        // VFS fallback: if the script is not on disk (standalone / pak mode),
        // extract it from the VFS to a temp directory before handing it to Python.
        if (!std::filesystem::exists(scriptFullPath))
        {
            auto vfsPath = "/project/" + sc.ScriptPath;
            auto data = FileSystem::Get().Read(vfsPath);
            if (data)
            {
                auto tempDir = std::filesystem::temp_directory_path() / "CandyGame";
                scriptFullPath = tempDir / sc.ScriptPath;
                std::filesystem::create_directories(scriptFullPath.parent_path());
                {
                    std::ofstream out(scriptFullPath, std::ios::binary);
                    out.write(reinterpret_cast<const char*>(data->data()), data->size());
                }
                CANDY_CORE_INFO("Extracted script to temp: {0}", scriptFullPath.string());

                // Add the temp scripts dir to Python's sys.path once (for import statements)
                if (m_TempScriptsDir.empty())
                {
                    m_TempScriptsDir = tempDir;
                    py::module_ sys = py::module_::import("sys");
                    sys.attr("path").attr("append")(m_TempScriptsDir.string());
                }
            }
            else
            {
                CANDY_CORE_ERROR("Script file not found: {0} (also tried VFS {1})", ResolveScriptPath(sc.ScriptPath).string(), vfsPath);
                return;
            }
        }

        // Use importlib to load the script by file path (bypasses sys.path)
        py::module_ util = py::module_::import("importlib.util");
        py::object spec = util.attr("spec_from_file_location")(sc.ClassName, scriptFullPath.string());
        if (spec.is_none())
        {
            CANDY_CORE_ERROR("Failed to create module spec for '{0}'", scriptFullPath.string());
            return;
        }
        py::object pyModule = util.attr("module_from_spec")(spec);
        spec.attr("loader").attr("exec_module")(pyModule);

        // ClassName is the actual Python class name
        py::object pyClass = pyModule.attr(sc.ClassName.c_str());
        py::object pyInstance = pyClass();

        auto instance = std::make_unique<ScriptInstance>();
        instance->PyObject = pyInstance;
        instance->StoredEntity = entity;

        auto* scriptObj = pyInstance.cast<ScriptObject*>();
        scriptObj->m_Entity = &instance->StoredEntity;
        instance->Script = scriptObj;

        m_Instances[uuid] = std::move(instance);

        scriptObj->OnConstruct();
    }
    catch (const py::error_already_set& e)
    {
        CANDY_CORE_ERROR("Python error instantiating script '{0}': {1}", sc.ClassName, e.what());
    }
    catch (const std::exception& e)
    {
        CANDY_CORE_ERROR("Failed to instantiate script '{0}': {1}", sc.ClassName, e.what());
    }
}

void ScriptSystem::DestroyScript(UUID entityID)
{
    auto it = m_Instances.find(entityID);
    if (it != m_Instances.end())
    {
        if (it->second->Script)
            it->second->Script->OnDestroy();
        m_Instances.erase(it);
    }
}

void ScriptSystem::OnRuntimeStart()
{
    for (auto& [id, instance] : m_Instances)
    {
        if (instance->Script)
            instance->Script->OnStart();
    }
}

void ScriptSystem::OnUpdateRuntime(Timestep ts)
{
    for (auto& [id, instance] : m_Instances)
    {
        if (instance->Script)
            instance->Script->OnTick(ts);
    }
}

	void ScriptSystem::CallFunction(Entity entity, const std::string& funcName)
	{
		if (!entity.HasComponent<ScriptComponent>())
			return;
		auto uuid = entity.GetComponent<IDComponent>().ID;
		auto it = m_Instances.find(uuid);
		if (it == m_Instances.end())
			return;
		py::object pyObj = it->second->PyObject;
		if (py::hasattr(pyObj, funcName.c_str()))
			pyObj.attr(funcName.c_str())();
	}

	void ScriptSystem::OnRuntimeStop()
	{
    for (auto& [id, instance] : m_Instances)
    {
        if (instance->Script)
            instance->Script->OnDestroy();
    }
    m_Instances.clear();
}

}
