#pragma once

// ============================================================================
// CandyEngine Python Binding Annotation Macros
// ============================================================================
// These macros expand to nothing at compile time. They serve as metadata
// markers for generate_bindings.py, which scans C++ headers to auto-generate
// pybind11 binding code (ScriptBindings.generated.inl) and Python type stubs
// (candy.pyi).
//
// Usage (inline macros, generator scans for these patterns — names are
// inferred from the declaration that follows the macro):
//
//   CANDY_CLASS()
//   struct TransformComponent
//   {
//       CANDY_PROPERTY()
//       glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
//       CANDY_PROPERTY()
//       glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
//   };
//
//   struct Rigidbody2DComponent
//   {
//       CANDY_ENUM()
//       enum class BodyType { Static = 0, Dynamic, Kinematic };
//   };
//
// Notes:
//   * CANDY_PROPERTY() must sit directly above the member it binds (no blank
//     line / non-comment line in between).
//   * CANDY_ENUM() auto-detects its values from the enum body.
//   * Nested enums get the parent struct name prefixed automatically
//     (e.g. Rigidbody2DComponent::BodyType).
// ============================================================================

#define CANDY_CLASS(...)
#define CANDY_PROPERTY(...)
#define CANDY_FUNCTION(...)
#define CANDY_ENUM(...)
#define CANDY_ENUM_VALUE(...)
