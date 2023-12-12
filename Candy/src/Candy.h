#pragma once

//for use by Candy application

#include "Candy/Core/Base.h"
#include "Candy/Core/Application.h"
#include "Candy/Core/Log.h"
#include "Candy/Core/Layer.h"
#include "Candy/Core/Input.h"
#include "Candy/Core/KeyCodes.h"
#include "Candy/Core/MouseCodes.h"
#include "Candy/Core/Timestep.h"

#include "Candy/Imgui/ImguiLayer.h"

#include "Candy/Scene/Scene.h"
#include "Candy/Scene/Entity.h"
#include "Candy/Scene/Components.h"
#include "Candy/Scene/ScriptableEntity.h"

//-------------- Renderer ---------------
#include "Candy/Renderer/Renderer.h"
#include "Candy/Renderer/Renderer2D.h"
#include "Candy/Renderer/RenderCommand.h" 
#include "Candy/Renderer/Buffer.h"
#include "Candy/Renderer/Shader.h"
#include "Candy/Renderer/Texture.h"
#include "Candy/Renderer/VertexArray.h"
#include "Candy/Renderer/Framebuffer.h"
#include "Candy/Renderer/OrthographicCamera.h"
#include "Candy/Renderer/OrthographicCameraController.h"