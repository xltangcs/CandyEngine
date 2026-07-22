#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <filesystem>

#include <glm/glm.hpp>

#include "Runtime/Scripting/ScriptObject.h"
#include "Runtime/Scripting/ScriptSystem.h"
#include "Runtime/Scene/Entity.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Core/Input.h"
#include "Runtime/Core/KeyCodes.h"

#include "Runtime/Audio/AudioEngine.h"
#include "Runtime/Project/ProjectUtils.h"
#include "Runtime/Core/FileSystem.h"

#include "box2d/b2_body.h"

namespace py = pybind11;

class PyScriptObject : public Candy::ScriptObject {
public:
    using Candy::ScriptObject::ScriptObject;

    void OnConstruct() override
    {
        PYBIND11_OVERRIDE_NAME(void, Candy::ScriptObject, "on_construct", OnConstruct);
    }

    void OnStart() override
    {
        PYBIND11_OVERRIDE_NAME(void, Candy::ScriptObject, "on_start", OnStart);
    }

    void OnTick(Candy::Timestep ts) override
    {
        PYBIND11_OVERRIDE_NAME(void, Candy::ScriptObject, "on_tick", OnTick, (float)ts);
    }

	void OnDestroy() override
	{
		PYBIND11_OVERRIDE_NAME(void, Candy::ScriptObject, "on_destroy", OnDestroy);
	}

	void OnCollisionEnter(const Candy::Entity& other) override
	{
		PYBIND11_OVERRIDE_NAME(void, Candy::ScriptObject, "on_collision_enter", OnCollisionEnter, other);
	}

	void OnCollisionExit(const Candy::Entity& other) override
	{
		PYBIND11_OVERRIDE_NAME(void, Candy::ScriptObject, "on_collision_exit", OnCollisionExit, other);
	}
};

namespace {

py::object GetComponentFromEntity(Candy::Entity& entity, const std::string& type)
{
	if (type == "TransformComponent")
		return py::cast(&entity.GetComponent<Candy::TransformComponent>(), py::return_value_policy::reference);

	if (type == "UITextBlockComponent")
		return py::cast(&entity.GetComponent<Candy::UITextBlockComponent>(), py::return_value_policy::reference);

	if (type == "UIButtonComponent")
		return py::cast(&entity.GetComponent<Candy::UIButtonComponent>(), py::return_value_policy::reference);

	if (type == "Rigidbody2DComponent")
		return py::cast(&entity.GetComponent<Candy::Rigidbody2DComponent>(), py::return_value_policy::reference);

	if (type == "BoxCollider2DComponent")
		return py::cast(&entity.GetComponent<Candy::BoxCollider2DComponent>(), py::return_value_policy::reference);

	if (type == "SpriteRendererComponent")
		return py::cast(&entity.GetComponent<Candy::SpriteRendererComponent>(), py::return_value_policy::reference);

	if (type == "ScriptComponent")
		return py::cast(&entity.GetComponent<Candy::ScriptComponent>(), py::return_value_policy::reference);

	if (type == "TagComponent")
		return py::cast(&entity.GetComponent<Candy::TagComponent>(), py::return_value_policy::reference);

	if (type == "AudioSourceComponent")
		return py::cast(&entity.GetComponent<Candy::AudioSourceComponent>(), py::return_value_policy::reference);

	throw std::runtime_error("Unknown component type: " + type);
}

bool EntityHasComponent(Candy::Entity& entity, const std::string& type)
{
	if (type == "TransformComponent")
		return entity.HasComponent<Candy::TransformComponent>();

	if (type == "UITextBlockComponent")
		return entity.HasComponent<Candy::UITextBlockComponent>();

	if (type == "UIButtonComponent")
		return entity.HasComponent<Candy::UIButtonComponent>();

	if (type == "Rigidbody2DComponent")
		return entity.HasComponent<Candy::Rigidbody2DComponent>();

	if (type == "BoxCollider2DComponent")
		return entity.HasComponent<Candy::BoxCollider2DComponent>();

	if (type == "SpriteRendererComponent")
		return entity.HasComponent<Candy::SpriteRendererComponent>();

	if (type == "ScriptComponent")
		return entity.HasComponent<Candy::ScriptComponent>();

	if (type == "TagComponent")
		return entity.HasComponent<Candy::TagComponent>();

	if (type == "AudioSourceComponent")
		return entity.HasComponent<Candy::AudioSourceComponent>();

	return false;
}

void AddComponentToEntity(Candy::Entity& entity, const std::string& type)
{
	if (type == "Rigidbody2DComponent")
	{
		Candy::Rigidbody2DComponent comp;
		comp.Type = Candy::Rigidbody2DComponent::BodyType::Dynamic;
		entity.AddComponent<Candy::Rigidbody2DComponent>(comp);
	}
	else if (type == "BoxCollider2DComponent")
	{
		entity.AddComponent<Candy::BoxCollider2DComponent>();
	}
	else if (type == "CircleCollider2DComponent")
	{
		entity.AddComponent<Candy::CircleCollider2DComponent>();
	}
	else if (type == "SpriteRendererComponent")
	{
		entity.AddComponent<Candy::SpriteRendererComponent>();
	}
	else if (type == "ScriptComponent")
	{
		entity.AddComponent<Candy::ScriptComponent>();
	}
	else if (type == "TagComponent")
	{
		entity.AddComponent<Candy::TagComponent>();
	}
	else if (type == "AudioSourceComponent")
	{
		entity.AddComponent<Candy::AudioSourceComponent>();
	}
	else
	{
		throw std::runtime_error("Cannot add component of type: " + type);
	}
}

bool PyIsKeyPressed(const std::string& key)
{
    static const std::unordered_map<std::string, Candy::KeyCode> s_KeyMap = {
        {"SPACE",    Candy::Key::Space},
        {"W",        Candy::Key::W},
        {"A",        Candy::Key::A},
        {"S",        Candy::Key::S},
        {"D",        Candy::Key::D},
        {"UP",       Candy::Key::Up},
        {"DOWN",     Candy::Key::Down},
        {"LEFT",     Candy::Key::Left},
        {"RIGHT",    Candy::Key::Right},
        {"ESCAPE",   Candy::Key::Escape},
        {"ENTER",    Candy::Key::Enter},
        {"TAB",      Candy::Key::Tab},
        {"SHIFT",    Candy::Key::LeftShift},
        {"CONTROL",  Candy::Key::LeftControl},
        {"ALT",      Candy::Key::LeftAlt},
    };

    auto it = s_KeyMap.find(key);
    if (it != s_KeyMap.end())
        return Candy::Input::IsKeyPressed(it->second);

    CANDY_CORE_WARN("Python is_key_pressed: unknown key '{0}'", key);
    return false;
}

} // anonymous namespace

// Called from ScriptSystem::InitPython() to force linker to include this translation unit
void PyBindings_ForceLink() {}

PYBIND11_EMBEDDED_MODULE(candy, m)
{
    m.doc() = "CandyEngine Python Scripting API";

    // --- Vec3 ---
    py::class_<glm::vec3>(m, "Vec3")
        .def(py::init<>())
        .def(py::init<float, float, float>())
        .def_readwrite("x", &glm::vec3::x)
        .def_readwrite("y", &glm::vec3::y)
        .def_readwrite("z", &glm::vec3::z)
        .def("__repr__", [](const glm::vec3& v) {
            return "Vec3(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        });

    // --- Vec4 ---
    py::class_<glm::vec4>(m, "Vec4")
        .def(py::init<>())
        .def(py::init<float, float, float, float>())
        .def_readwrite("r", &glm::vec4::r)
        .def_readwrite("g", &glm::vec4::g)
        .def_readwrite("b", &glm::vec4::b)
        .def_readwrite("a", &glm::vec4::a)
        .def("__repr__", [](const glm::vec4& v) {
            return "Vec4(" + std::to_string(v.r) + ", " + std::to_string(v.g) + ", " + std::to_string(v.b) + ", " + std::to_string(v.a) + ")";
        });

    // --- Vec2 ---
    py::class_<glm::vec2>(m, "Vec2")
        .def(py::init<>())
        .def(py::init<float, float>())
        .def_readwrite("x", &glm::vec2::x)
        .def_readwrite("y", &glm::vec2::y)
        .def("__repr__", [](const glm::vec2& v) {
            return "Vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
        });

    // --- TransformComponent ---
    py::class_<Candy::TransformComponent>(m, "TransformComponent")
        .def(py::init<>())
        .def_readwrite("Translation", &Candy::TransformComponent::Translation)
        .def_readwrite("Rotation",    &Candy::TransformComponent::Rotation)
        .def_readwrite("Scale",       &Candy::TransformComponent::Scale);

    // --- TextBlockUIData ---
    py::class_<Candy::TextBlockUIData>(m, "TextBlockUIData")
        .def(py::init<>())
        .def_readwrite("Text",     &Candy::TextBlockUIData::Text)
        .def_readwrite("Color",    &Candy::TextBlockUIData::Color)
        .def_readwrite("Position", &Candy::TextBlockUIData::Position)
        .def_readwrite("FontSize", &Candy::TextBlockUIData::FontSize)
        .def_readwrite("Visible",  &Candy::TextBlockUIData::Visible);

    // --- UITextBlockComponent ---
    py::class_<Candy::UITextBlockComponent>(m, "UITextBlockComponent")
        .def(py::init<>())
        .def_readwrite("TextBlockDatas", &Candy::UITextBlockComponent::TextBlockDatas)
        .def("set_text_visible", [](Candy::UITextBlockComponent& self, const std::string& key, bool visible) {
            auto it = self.TextBlockDatas.find(key);
            if (it != self.TextBlockDatas.end()) it->second.Visible = visible;
        }, py::arg("key"), py::arg("visible"))
        .def("set_text", [](Candy::UITextBlockComponent& self, const std::string& key, const std::string& text) {
            auto it = self.TextBlockDatas.find(key);
            if (it != self.TextBlockDatas.end()) it->second.Text = text;
        }, py::arg("key"), py::arg("text"));

    // --- ButtonUIData ---
    py::class_<Candy::ButtonUIData>(m, "ButtonUIData")
        .def(py::init<>())
        .def_readwrite("Text",     &Candy::ButtonUIData::Text)
        .def_readwrite("FontSize", &Candy::ButtonUIData::FontSize)
        .def_readwrite("Size",     &Candy::ButtonUIData::Size)
        .def_readwrite("Position", &Candy::ButtonUIData::Position)
        .def_readwrite("OnClick",  &Candy::ButtonUIData::OnClick)
        .def_readwrite("Visible",  &Candy::ButtonUIData::Visible);

    // --- UIButtonComponent ---
    // NOTE: reading a std::unordered_map member returns a COPY in pybind11, so
    // mutating ButtonDatas[key] from Python would not persist. Use the helper
    // methods below, which mutate the C++ data in place.
    py::class_<Candy::UIButtonComponent>(m, "UIButtonComponent")
        .def(py::init<>())
        .def("set_button_visible", [](Candy::UIButtonComponent& self, const std::string& key, bool visible) {
            auto it = self.ButtonDatas.find(key);
            if (it != self.ButtonDatas.end()) it->second.Visible = visible;
        }, py::arg("key"), py::arg("visible"),
             "Show/hide a button by its key.")
        .def("get_button_visible", [](Candy::UIButtonComponent& self, const std::string& key) -> bool {
            auto it = self.ButtonDatas.find(key);
            return it != self.ButtonDatas.end() ? it->second.Visible : false;
        }, py::arg("key"))
        .def("set_button_text", [](Candy::UIButtonComponent& self, const std::string& key, const std::string& text) {
            auto it = self.ButtonDatas.find(key);
            if (it != self.ButtonDatas.end()) it->second.Text = text;
        }, py::arg("key"), py::arg("text"))
        .def("set_button_onclick", [](Candy::UIButtonComponent& self, const std::string& key, const std::string& onclick) {
            auto it = self.ButtonDatas.find(key);
            if (it != self.ButtonDatas.end()) it->second.OnClick = onclick;
        }, py::arg("key"), py::arg("onclick"));

    // --- Rigidbody2DComponent ---
    py::class_<Candy::Rigidbody2DComponent>(m, "Rigidbody2DComponent")
        .def(py::init<>())
        .def_property("Type",
            [](Candy::Rigidbody2DComponent& self) -> int { return static_cast<int>(self.Type); },
            [](Candy::Rigidbody2DComponent& self, int type) { self.Type = static_cast<Candy::Rigidbody2DComponent::BodyType>(type); })
        .def_readwrite("FixedRotation", &Candy::Rigidbody2DComponent::FixedRotation)
        .def("get_linear_velocity", [](Candy::Rigidbody2DComponent& self) -> glm::vec2 {
            b2Body* body = static_cast<b2Body*>(self.RuntimeBody);
            if (!body) return {0.0f, 0.0f};
            auto v = body->GetLinearVelocity();
            return {v.x, v.y};
        })
        .def("set_linear_velocity", [](Candy::Rigidbody2DComponent& self, float vx, float vy) {
            b2Body* body = static_cast<b2Body*>(self.RuntimeBody);
            if (body) body->SetLinearVelocity({vx, vy});
        })
        .def("apply_linear_impulse", [](Candy::Rigidbody2DComponent& self, float ix, float iy) {
            b2Body* body = static_cast<b2Body*>(self.RuntimeBody);
            if (body) body->ApplyLinearImpulse({ix, iy}, body->GetWorldCenter(), true);
        });

    // --- BoxCollider2DComponent ---
    py::class_<Candy::BoxCollider2DComponent>(m, "BoxCollider2DComponent")
        .def(py::init<>())
        .def_readwrite("Offset", &Candy::BoxCollider2DComponent::Offset)
        .def_readwrite("Size", &Candy::BoxCollider2DComponent::Size)
        .def_readwrite("Density", &Candy::BoxCollider2DComponent::Density)
        .def_readwrite("Friction", &Candy::BoxCollider2DComponent::Friction)
        .def_readwrite("Restitution", &Candy::BoxCollider2DComponent::Restitution);

    // --- CircleCollider2DComponent ---
    py::class_<Candy::CircleCollider2DComponent>(m, "CircleCollider2DComponent")
        .def(py::init<>())
        .def_readwrite("Offset", &Candy::CircleCollider2DComponent::Offset)
        .def_readwrite("Radius", &Candy::CircleCollider2DComponent::Radius)
        .def_readwrite("Density", &Candy::CircleCollider2DComponent::Density)
        .def_readwrite("Friction", &Candy::CircleCollider2DComponent::Friction)
        .def_readwrite("Restitution", &Candy::CircleCollider2DComponent::Restitution);

    // --- SpriteRendererComponent ---
    py::class_<Candy::SpriteRendererComponent>(m, "SpriteRendererComponent")
        .def(py::init<>())
        .def_readwrite("Color", &Candy::SpriteRendererComponent::Color)
        .def_readwrite("TexturePath", &Candy::SpriteRendererComponent::TexturePath)
        .def_readwrite("TilingFactor", &Candy::SpriteRendererComponent::TilingFactor);

    // --- ScriptComponent ---
    py::class_<Candy::ScriptComponent>(m, "ScriptComponent")
        .def(py::init<>())
        .def_readwrite("ScriptPath", &Candy::ScriptComponent::ScriptPath)
        .def_readwrite("ClassName", &Candy::ScriptComponent::ClassName);

    // --- TagComponent ---
    py::class_<Candy::TagComponent>(m, "TagComponent")
        .def(py::init<>())
        .def_readwrite("Tag", &Candy::TagComponent::Tag);

    // --- AudioSourceComponent ---
    py::class_<Candy::AudioSourceComponent>(m, "AudioSourceComponent")
        .def(py::init<>())
        .def_readwrite("SoundPath", &Candy::AudioSourceComponent::SoundPath)
        .def_readwrite("Volume", &Candy::AudioSourceComponent::Volume)
        .def_readwrite("Looping", &Candy::AudioSourceComponent::Looping)
        .def_readwrite("PlayOnStart", &Candy::AudioSourceComponent::PlayOnStart);

    // --- Entity ---
    py::class_<Candy::Entity>(m, "Entity")
        .def("get_component", &GetComponentFromEntity, py::arg("type"),
             "Get a component by type name (e.g. \"TransformComponent\")")
        .def("has_component", &EntityHasComponent, py::arg("type"),
             "Check if this entity has a component by type name")
        .def("add_component", &AddComponentToEntity, py::arg("type"),
             "Add a component by type name (e.g. \"Rigidbody2DComponent\")")
        .def("queue_free", &Candy::Entity::QueueFree,
             "Deferred deletion (Godot-style): destroyed on the next safe frame. Safe to call from on_tick / collision callbacks.")
        .def("is_queued_for_deletion", &Candy::Entity::IsQueuedForDeletion,
             "Returns true if this entity has been marked for deferred deletion via queue_free().")
        .def("call_function", [](Candy::Entity& self, const std::string& funcName) {
            Candy::ScriptSystem::Get().CallFunction(self, funcName);
        }, py::arg("func_name"),
             "Call a Python method by name on this entity's script instance (cross-entity messaging).")
        .def_property("tag",
            [](Candy::Entity& self) -> std::string {
                return self.GetComponent<Candy::TagComponent>().Tag;
            },
            [](Candy::Entity& self, const std::string& tag) {
                self.GetComponent<Candy::TagComponent>().Tag = tag;
            })
        .def_property_readonly("scene", [](Candy::Entity& self) -> py::object {
            return py::cast(self.GetScene(), py::return_value_policy::reference);
        });

    // --- ScriptObject ---
    py::class_<Candy::ScriptObject, PyScriptObject>(m, "ScriptObject")
        .def(py::init<>())
        .def("on_construct",        &Candy::ScriptObject::OnConstruct)
        .def("on_start",            &Candy::ScriptObject::OnStart)
        .def("on_destroy",          &Candy::ScriptObject::OnDestroy)
        .def("on_collision_enter",  &Candy::ScriptObject::OnCollisionEnter)
        .def("on_collision_exit",   &Candy::ScriptObject::OnCollisionExit)
        .def_property_readonly("_entity", [](Candy::ScriptObject& self) -> py::object {
            Candy::Entity* entity = self.GetEntity();
            if (entity)
                return py::cast(entity, py::return_value_policy::reference);
            return py::none();
        });

    // --- Scene ---
    py::class_<Candy::Scene>(m, "Scene")
        .def("find_entity_by_tag", [](Candy::Scene& self, const std::string& tag) -> py::object {
            auto view = self.GetAllEntitiesWith<Candy::TagComponent>();
            for (auto e : view)
            {
                if (view.get<Candy::TagComponent>(e).Tag == tag)
                    return py::cast(Candy::Entity{e, &self}, py::return_value_policy::reference);
            }
            return py::none();
        })
        .def("create_entity", [](Candy::Scene& self, const std::string& name) -> Candy::Entity {
            return self.CreateEntity(name);
        })
        .def("destroy_entity", [](Candy::Scene& self, Candy::Entity& entity) {
            self.DestroyEntity(entity);
        })
        .def("queue_free", [](Candy::Scene& self, Candy::Entity& entity) {
            self.QueueFree(entity);
        }, py::arg("entity"),
             "Deferred deletion (Godot-style): the entity is destroyed on the next safe frame.")
        .def("is_queued_for_deletion", [](Candy::Scene& self, Candy::Entity& entity) -> bool {
            return self.IsQueuedForDeletion(entity);
        }, py::arg("entity"))
		.def("create_physics_body", [](Candy::Scene& self, Candy::Entity& entity) {
			self.CreatePhysicsBody(entity);
		})
		.def("recreate_physics_body", [](Candy::Scene& self, Candy::Entity& entity) {
			self.RecreatePhysicsBody(entity);
		}, py::arg("entity"),
			"Destroy and recreate the entity's runtime physics body from its current Transform/Collider components (e.g. after resizing a collider).")
        .def("instantiate_script", [](Candy::Scene& self, Candy::Entity& entity) {
            Candy::ScriptSystem::Get().InstantiateScript(entity);
        });

    // --- Module-level functions ---
    m.def("is_key_pressed", &PyIsKeyPressed, py::arg("key"),
          "Check if a key is pressed. Key names: W, A, S, D, UP, DOWN, LEFT, RIGHT, SPACE, ESCAPE, etc.");

    m.def("play_one_shot", [](const std::string& path, float volume) {
        auto disk = Candy::FileSystem::Get().ResolveToDiskPath(path);
        if (disk)
            Candy::AudioEngine::PlayOneShot(disk->string(), volume);
        else
            CANDY_CORE_WARN("Python play_one_shot: could not resolve audio path '{0}'", path);
    }, py::arg("path"), py::arg("volume") = 1.0f,
       "Play a one-shot (non-looping) sound effect from a VFS:// path.");
}
