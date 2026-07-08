#include "CandyPCH.h"
#include "Candy/Scripting/ScriptSystem.h"

#include "Candy/Scripting/ScriptObject.h"
#include "Candy/Scene/Entity.h"
#include "Candy/Scene/Components.h"
#include "Candy/Core/Timestep.h"
#include "Candy/Core/UUID.h"

#include <pybind11/embed.h>

#include <filesystem>
#include <windows.h>

namespace py = pybind11;

extern void PyBindings_ForceLink();

namespace Candy {

static bool s_PythonInitialized = false;

struct ScriptInstance
{
    py::object PyObject;
    ScriptObject* Script = nullptr;
    Entity StoredEntity;
};

ScriptSystem::ScriptSystem()
{
}

ScriptSystem::~ScriptSystem()
{
    ShutdownPython();
}

void ScriptSystem::InitPython()
{
    if (s_PythonInitialized)
        return;

    ::PyBindings_ForceLink();  // Ensure PYBIND11_EMBEDDED_MODULE gets linked

    // Resolve scripts directory relative to the executable
    std::filesystem::path exePath = []() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf);
    }();
    std::filesystem::path scriptsDir = exePath.parent_path() // .../Candy_Editor/
        .parent_path()  // .../Debug-windows-x86_64/
        .parent_path()  // .../bin/
        .parent_path()  // <project_root>/
        / "assets" / "scripts";

    Py_Initialize();

    py::module_ sys = py::module_::import("sys");
    sys.attr("path").attr("append")(scriptsDir.string());

    s_PythonInitialized = true;
}

void ScriptSystem::ShutdownPython()
{
    if (!s_PythonInitialized)
        return;

    for (auto& [id, instance] : m_Instances)
    {
        if (instance->Script)
            instance->Script->OnDestroy();
    }
    m_Instances.clear();

    Py_Finalize();
    s_PythonInitialized = false;
}

void ScriptSystem::InstantiateScript(Entity& entity)
{
    UUID uuid = entity.GetUUID();
    auto& sc = entity.GetComponent<ScriptComponent>();
    const std::string& className = sc.ClassName;

    if (m_Instances.find(uuid) != m_Instances.end())
    {
        CANDY_CORE_WARN("Script already instantiated for entity {0}", (uint64_t)uuid);
        return;
    }

    try
    {
        py::object pyModule = py::module_::import(className.c_str());

        // Conventions: ClassName field is lowercase filename, Python class is capitalized
        std::string classAttr = className;
        if (!classAttr.empty())
            classAttr[0] = (char)std::toupper((unsigned char)classAttr[0]);
        py::object pyClass = pyModule.attr(classAttr.c_str());
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
        CANDY_CORE_ERROR("Python error instantiating script '{0}': {1}", className, e.what());
    }
    catch (const std::exception& e)
    {
        CANDY_CORE_ERROR("Failed to instantiate script '{0}': {1}", className, e.what());
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
