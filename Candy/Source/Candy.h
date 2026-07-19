#pragma once

//for use by Candy application

#include "Runtime/Core/Assert.h"
#include "Runtime/Core/Base.h"
#include "Runtime/Core/Application.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Core/Layer.h"
#include "Runtime/Core/Input.h"
#include "Runtime/Core/KeyCodes.h"
#include "Runtime/Core/MouseCodes.h"
#include "Runtime/Core/Timestep.h"

#include "Runtime/Imgui/ImguiLayer.h"

#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Entity.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Scene/ScriptableEntity.h"

//-------------- Project ---------------
#include "Runtime/Project/Project.h"

//-------------- Audio ---------------
#include "Runtime/Audio/AudioEngine.h"

//-------------- Renderer ---------------
#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Renderer/Renderer2D.h"
#include "Runtime/Renderer/RenderCommand.h" 
#include "Runtime/Renderer/Buffer.h"
#include "Runtime/Renderer/Shader.h"
#include "Runtime/Renderer/Texture.h"
#include "Runtime/Renderer/VertexArray.h"
#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/OrthographicCamera.h"
#include "Runtime/Renderer/OrthographicCameraController.h"