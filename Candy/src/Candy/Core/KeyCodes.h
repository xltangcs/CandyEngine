#pragma once

namespace Candy
{
	typedef enum class KeyCode : uint16_t
	{
		// From glfw3.h
		Space = 32,
		Apostrophe = 39, /* ' */
		Comma = 44, /* , */
		Minus = 45, /* - */
		Period = 46, /* . */
		Slash = 47, /* / */

		D0 = 48, /* 0 */
		D1 = 49, /* 1 */
		D2 = 50, /* 2 */
		D3 = 51, /* 3 */
		D4 = 52, /* 4 */
		D5 = 53, /* 5 */
		D6 = 54, /* 6 */
		D7 = 55, /* 7 */
		D8 = 56, /* 8 */
		D9 = 57, /* 9 */

		Semicolon = 59, /* ; */
		Equal = 61, /* = */

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,  /* [ */
		Backslash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		/* Function keys */
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		/* Keypad */
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	} Key;

	inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
	{
		os << static_cast<int32_t>(keyCode);
		return os;
	}
}

// From glfw3.h
#define CANDY_KEY_SPACE           ::Candy::Key::Space
#define CANDY_KEY_APOSTROPHE      ::Candy::Key::Apostrophe    /* ' */
#define CANDY_KEY_COMMA           ::Candy::Key::Comma         /* , */
#define CANDY_KEY_MINUS           ::Candy::Key::Minus         /* - */
#define CANDY_KEY_PERIOD          ::Candy::Key::Period        /* . */
#define CANDY_KEY_SLASH           ::Candy::Key::Slash         /* / */
#define CANDY_KEY_0               ::Candy::Key::D0
#define CANDY_KEY_1               ::Candy::Key::D1
#define CANDY_KEY_2               ::Candy::Key::D2
#define CANDY_KEY_3               ::Candy::Key::D3
#define CANDY_KEY_4               ::Candy::Key::D4
#define CANDY_KEY_5               ::Candy::Key::D5
#define CANDY_KEY_6               ::Candy::Key::D6
#define CANDY_KEY_7               ::Candy::Key::D7
#define CANDY_KEY_8               ::Candy::Key::D8
#define CANDY_KEY_9               ::Candy::Key::D9
#define CANDY_KEY_SEMICOLON       ::Candy::Key::Semicolon     /* ; */
#define CANDY_KEY_EQUAL           ::Candy::Key::Equal         /* = */
#define CANDY_KEY_A               ::Candy::Key::A
#define CANDY_KEY_B               ::Candy::Key::B
#define CANDY_KEY_C               ::Candy::Key::C
#define CANDY_KEY_D               ::Candy::Key::D
#define CANDY_KEY_E               ::Candy::Key::E
#define CANDY_KEY_F               ::Candy::Key::F
#define CANDY_KEY_G               ::Candy::Key::G
#define CANDY_KEY_H               ::Candy::Key::H
#define CANDY_KEY_I               ::Candy::Key::I
#define CANDY_KEY_J               ::Candy::Key::J
#define CANDY_KEY_K               ::Candy::Key::K
#define CANDY_KEY_L               ::Candy::Key::L
#define CANDY_KEY_M               ::Candy::Key::M
#define CANDY_KEY_N               ::Candy::Key::N
#define CANDY_KEY_O               ::Candy::Key::O
#define CANDY_KEY_P               ::Candy::Key::P
#define CANDY_KEY_Q               ::Candy::Key::Q
#define CANDY_KEY_R               ::Candy::Key::R
#define CANDY_KEY_S               ::Candy::Key::S
#define CANDY_KEY_T               ::Candy::Key::T
#define CANDY_KEY_U               ::Candy::Key::U
#define CANDY_KEY_V               ::Candy::Key::V
#define CANDY_KEY_W               ::Candy::Key::W
#define CANDY_KEY_X               ::Candy::Key::X
#define CANDY_KEY_Y               ::Candy::Key::Y
#define CANDY_KEY_Z               ::Candy::Key::Z
#define CANDY_KEY_LEFT_BRACKET    ::Candy::Key::LeftBracket   /* [ */
#define CANDY_KEY_BACKSLASH       ::Candy::Key::Backslash     /* \ */
#define CANDY_KEY_RIGHT_BRACKET   ::Candy::Key::RightBracket  /* ] */
#define CANDY_KEY_GRAVE_ACCENT    ::Candy::Key::GraveAccent   /* ` */
#define CANDY_KEY_WORLD_1         ::Candy::Key::World1        /* non-US #1 */
#define CANDY_KEY_WORLD_2         ::Candy::Key::World2        /* non-US #2 */

/* Function keys */
#define CANDY_KEY_ESCAPE          ::Candy::Key::Escape
#define CANDY_KEY_ENTER           ::Candy::Key::Enter
#define CANDY_KEY_TAB             ::Candy::Key::Tab
#define CANDY_KEY_BACKSPACE       ::Candy::Key::Backspace
#define CANDY_KEY_INSERT          ::Candy::Key::Insert
#define CANDY_KEY_DELETE          ::Candy::Key::Delete
#define CANDY_KEY_RIGHT           ::Candy::Key::Right
#define CANDY_KEY_LEFT            ::Candy::Key::Left
#define CANDY_KEY_DOWN            ::Candy::Key::Down
#define CANDY_KEY_UP              ::Candy::Key::Up
#define CANDY_KEY_PAGE_UP         ::Candy::Key::PageUp
#define CANDY_KEY_PAGE_DOWN       ::Candy::Key::PageDown
#define CANDY_KEY_HOME            ::Candy::Key::Home
#define CANDY_KEY_END             ::Candy::Key::End
#define CANDY_KEY_CAPS_LOCK       ::Candy::Key::CapsLock
#define CANDY_KEY_SCROLL_LOCK     ::Candy::Key::ScrollLock
#define CANDY_KEY_NUM_LOCK        ::Candy::Key::NumLock
#define CANDY_KEY_PRINT_SCREEN    ::Candy::Key::PrintScreen
#define CANDY_KEY_PAUSE           ::Candy::Key::Pause
#define CANDY_KEY_F1              ::Candy::Key::F1
#define CANDY_KEY_F2              ::Candy::Key::F2
#define CANDY_KEY_F3              ::Candy::Key::F3
#define CANDY_KEY_F4              ::Candy::Key::F4
#define CANDY_KEY_F5              ::Candy::Key::F5
#define CANDY_KEY_F6              ::Candy::Key::F6
#define CANDY_KEY_F7              ::Candy::Key::F7
#define CANDY_KEY_F8              ::Candy::Key::F8
#define CANDY_KEY_F9              ::Candy::Key::F9
#define CANDY_KEY_F10             ::Candy::Key::F10
#define CANDY_KEY_F11             ::Candy::Key::F11
#define CANDY_KEY_F12             ::Candy::Key::F12
#define CANDY_KEY_F13             ::Candy::Key::F13
#define CANDY_KEY_F14             ::Candy::Key::F14
#define CANDY_KEY_F15             ::Candy::Key::F15
#define CANDY_KEY_F16             ::Candy::Key::F16
#define CANDY_KEY_F17             ::Candy::Key::F17
#define CANDY_KEY_F18             ::Candy::Key::F18
#define CANDY_KEY_F19             ::Candy::Key::F19
#define CANDY_KEY_F20             ::Candy::Key::F20
#define CANDY_KEY_F21             ::Candy::Key::F21
#define CANDY_KEY_F22             ::Candy::Key::F22
#define CANDY_KEY_F23             ::Candy::Key::F23
#define CANDY_KEY_F24             ::Candy::Key::F24
#define CANDY_KEY_F25             ::Candy::Key::F25

/* Keypad */
#define CANDY_KEY_KP_0            ::Candy::Key::KP0
#define CANDY_KEY_KP_1            ::Candy::Key::KP1
#define CANDY_KEY_KP_2            ::Candy::Key::KP2
#define CANDY_KEY_KP_3            ::Candy::Key::KP3
#define CANDY_KEY_KP_4            ::Candy::Key::KP4
#define CANDY_KEY_KP_5            ::Candy::Key::KP5
#define CANDY_KEY_KP_6            ::Candy::Key::KP6
#define CANDY_KEY_KP_7            ::Candy::Key::KP7
#define CANDY_KEY_KP_8            ::Candy::Key::KP8
#define CANDY_KEY_KP_9            ::Candy::Key::KP9
#define CANDY_KEY_KP_DECIMAL      ::Candy::Key::KPDecimal
#define CANDY_KEY_KP_DIVIDE       ::Candy::Key::KPDivide
#define CANDY_KEY_KP_MULTIPLY     ::Candy::Key::KPMultiply
#define CANDY_KEY_KP_SUBTRACT     ::Candy::Key::KPSubtract
#define CANDY_KEY_KP_ADD          ::Candy::Key::KPAdd
#define CANDY_KEY_KP_ENTER        ::Candy::Key::KPEnter
#define CANDY_KEY_KP_EQUAL        ::Candy::Key::KPEqual

#define CANDY_KEY_LEFT_SHIFT      ::Candy::Key::LeftShift
#define CANDY_KEY_LEFT_CONTROL    ::Candy::Key::LeftControl
#define CANDY_KEY_LEFT_ALT        ::Candy::Key::LeftAlt
#define CANDY_KEY_LEFT_SUPER      ::Candy::Key::LeftSuper
#define CANDY_KEY_RIGHT_SHIFT     ::Candy::Key::RightShift
#define CANDY_KEY_RIGHT_CONTROL   ::Candy::Key::RightControl
#define CANDY_KEY_RIGHT_ALT       ::Candy::Key::RightAlt
#define CANDY_KEY_RIGHT_SUPER     ::Candy::Key::RightSuper
#define CANDY_KEY_MENU            ::Candy::Key::Menu