#pragma once

namespace Candy
{
	typedef enum class MouseCode : uint16_t
	{
		// From glfw3.h
		Button0 = 0,
		Button1 = 1,
		Button2 = 2,
		Button3 = 3,
		Button4 = 4,
		Button5 = 5,
		Button6 = 6,
		Button7 = 7,

		ButtonLast = Button7,
		ButtonLeft = Button0,
		ButtonRight = Button1,
		ButtonMiddle = Button2
	} Mouse;

	inline std::ostream& operator<<(std::ostream& os, MouseCode mouseCode)
	{
		os << static_cast<int32_t>(mouseCode);
		return os;
	}
}

#define CANDY_MOUSE_BUTTON_0      ::Candy::Mouse::Button0
#define CANDY_MOUSE_BUTTON_1      ::Candy::Mouse::Button1
#define CANDY_MOUSE_BUTTON_2      ::Candy::Mouse::Button2
#define CANDY_MOUSE_BUTTON_3      ::Candy::Mouse::Button3
#define CANDY_MOUSE_BUTTON_4      ::Candy::Mouse::Button4
#define CANDY_MOUSE_BUTTON_5      ::Candy::Mouse::Button5
#define CANDY_MOUSE_BUTTON_6      ::Candy::Mouse::Button6
#define CANDY_MOUSE_BUTTON_7      ::Candy::Mouse::Button7
#define CANDY_MOUSE_BUTTON_LAST   ::Candy::Mouse::ButtonLast
#define CANDY_MOUSE_BUTTON_LEFT   ::Candy::Mouse::ButtonLeft
#define CANDY_MOUSE_BUTTON_RIGHT  ::Candy::Mouse::ButtonRight
#define CANDY_MOUSE_BUTTON_MIDDLE ::Candy::Mouse::ButtonMiddle
#define CANDY_MOUSE_BUTTON_MIDDLE ::Candy::Mouse::ButtonMiddle