#include "pch.h"
#include "ui/key_map.h"

namespace
{
    class key_map_t
    {
    public:
        key_map_t()
            : keys{}
        {
            this->keys[VK_BACK] = Noesis::Key_Back;
            this->keys[VK_TAB] = Noesis::Key_Tab;
            this->keys[VK_CLEAR] = Noesis::Key_Clear;
            this->keys[VK_RETURN] = Noesis::Key_Return;
            this->keys[VK_PAUSE] = Noesis::Key_Pause;

            this->keys[VK_SHIFT] = Noesis::Key_LeftShift;
            this->keys[VK_LSHIFT] = Noesis::Key_LeftShift;
            this->keys[VK_RSHIFT] = Noesis::Key_RightShift;
            this->keys[VK_CONTROL] = Noesis::Key_LeftCtrl;
            this->keys[VK_LCONTROL] = Noesis::Key_LeftCtrl;
            this->keys[VK_RCONTROL] = Noesis::Key_RightCtrl;
            this->keys[VK_MENU] = Noesis::Key_LeftAlt;
            this->keys[VK_LMENU] = Noesis::Key_LeftAlt;
            this->keys[VK_RMENU] = Noesis::Key_RightAlt;
            this->keys[VK_LWIN] = Noesis::Key_LWin;
            this->keys[VK_RWIN] = Noesis::Key_RWin;
            this->keys[VK_APPS] = Noesis::Key_Apps;
            this->keys[VK_SLEEP] = Noesis::Key_Sleep;
            this->keys[VK_ESCAPE] = Noesis::Key_Escape;

            this->keys[VK_SPACE] = Noesis::Key_Space;
            this->keys[VK_PRIOR] = Noesis::Key_Prior;
            this->keys[VK_NEXT] = Noesis::Key_Next;
            this->keys[VK_END] = Noesis::Key_End;
            this->keys[VK_HOME] = Noesis::Key_Home;
            this->keys[VK_LEFT] = Noesis::Key_Left;
            this->keys[VK_UP] = Noesis::Key_Up;
            this->keys[VK_RIGHT] = Noesis::Key_Right;
            this->keys[VK_DOWN] = Noesis::Key_Down;
            this->keys[VK_SELECT] = Noesis::Key_Select;
            this->keys[VK_PRINT] = Noesis::Key_Print;
            this->keys[VK_EXECUTE] = Noesis::Key_Execute;
            this->keys[VK_SNAPSHOT] = Noesis::Key_Snapshot;
            this->keys[VK_INSERT] = Noesis::Key_Insert;
            this->keys[VK_DELETE] = Noesis::Key_Delete;
            this->keys[VK_HELP] = Noesis::Key_Help;

            this->keys['0'] = Noesis::Key_D0;
            this->keys['1'] = Noesis::Key_D1;
            this->keys['2'] = Noesis::Key_D2;
            this->keys['3'] = Noesis::Key_D3;
            this->keys['4'] = Noesis::Key_D4;
            this->keys['5'] = Noesis::Key_D5;
            this->keys['6'] = Noesis::Key_D6;
            this->keys['7'] = Noesis::Key_D7;
            this->keys['8'] = Noesis::Key_D8;
            this->keys['9'] = Noesis::Key_D9;

            this->keys[VK_NUMPAD0] = Noesis::Key_NumPad0;
            this->keys[VK_NUMPAD1] = Noesis::Key_NumPad1;
            this->keys[VK_NUMPAD2] = Noesis::Key_NumPad2;
            this->keys[VK_NUMPAD3] = Noesis::Key_NumPad3;
            this->keys[VK_NUMPAD4] = Noesis::Key_NumPad4;
            this->keys[VK_NUMPAD5] = Noesis::Key_NumPad5;
            this->keys[VK_NUMPAD6] = Noesis::Key_NumPad6;
            this->keys[VK_NUMPAD7] = Noesis::Key_NumPad7;
            this->keys[VK_NUMPAD8] = Noesis::Key_NumPad8;
            this->keys[VK_NUMPAD9] = Noesis::Key_NumPad9;

            this->keys[VK_MULTIPLY] = Noesis::Key_Multiply;
            this->keys[VK_ADD] = Noesis::Key_Add;
            this->keys[VK_SEPARATOR] = Noesis::Key_Separator;
            this->keys[VK_SUBTRACT] = Noesis::Key_Subtract;
            this->keys[VK_DECIMAL] = Noesis::Key_Decimal;
            this->keys[VK_DIVIDE] = Noesis::Key_Divide;

            this->keys['A'] = Noesis::Key_A;
            this->keys['B'] = Noesis::Key_B;
            this->keys['C'] = Noesis::Key_C;
            this->keys['D'] = Noesis::Key_D;
            this->keys['E'] = Noesis::Key_E;
            this->keys['F'] = Noesis::Key_F;
            this->keys['G'] = Noesis::Key_G;
            this->keys['H'] = Noesis::Key_H;
            this->keys['I'] = Noesis::Key_I;
            this->keys['J'] = Noesis::Key_J;
            this->keys['K'] = Noesis::Key_K;
            this->keys['L'] = Noesis::Key_L;
            this->keys['M'] = Noesis::Key_M;
            this->keys['N'] = Noesis::Key_N;
            this->keys['O'] = Noesis::Key_O;
            this->keys['P'] = Noesis::Key_P;
            this->keys['Q'] = Noesis::Key_Q;
            this->keys['R'] = Noesis::Key_R;
            this->keys['S'] = Noesis::Key_S;
            this->keys['T'] = Noesis::Key_T;
            this->keys['U'] = Noesis::Key_U;
            this->keys['V'] = Noesis::Key_V;
            this->keys['W'] = Noesis::Key_W;
            this->keys['X'] = Noesis::Key_X;
            this->keys['Y'] = Noesis::Key_Y;
            this->keys['Z'] = Noesis::Key_Z;

            this->keys[VK_F1] = Noesis::Key_F1;
            this->keys[VK_F2] = Noesis::Key_F2;
            this->keys[VK_F3] = Noesis::Key_F3;
            this->keys[VK_F4] = Noesis::Key_F4;
            this->keys[VK_F5] = Noesis::Key_F5;
            this->keys[VK_F6] = Noesis::Key_F6;
            this->keys[VK_F7] = Noesis::Key_F7;
            this->keys[VK_F8] = Noesis::Key_F8;
            this->keys[VK_F9] = Noesis::Key_F9;
            this->keys[VK_F10] = Noesis::Key_F10;
            this->keys[VK_F11] = Noesis::Key_F11;
            this->keys[VK_F12] = Noesis::Key_F12;
            this->keys[VK_F13] = Noesis::Key_F13;
            this->keys[VK_F14] = Noesis::Key_F14;
            this->keys[VK_F15] = Noesis::Key_F15;
            this->keys[VK_F16] = Noesis::Key_F16;
            this->keys[VK_F17] = Noesis::Key_F17;
            this->keys[VK_F18] = Noesis::Key_F18;
            this->keys[VK_F19] = Noesis::Key_F19;
            this->keys[VK_F20] = Noesis::Key_F20;
            this->keys[VK_F21] = Noesis::Key_F21;
            this->keys[VK_F22] = Noesis::Key_F22;
            this->keys[VK_F23] = Noesis::Key_F23;
            this->keys[VK_F24] = Noesis::Key_F24;

            this->keys[VK_NUMLOCK] = Noesis::Key_NumLock;
            this->keys[VK_SCROLL] = Noesis::Key_Scroll;

            this->keys[VK_GAMEPAD_DPAD_LEFT] = Noesis::Key_GamepadLeft;
            this->keys[VK_GAMEPAD_DPAD_UP] = Noesis::Key_GamepadUp;
            this->keys[VK_GAMEPAD_DPAD_RIGHT] = Noesis::Key_GamepadRight;
            this->keys[VK_GAMEPAD_DPAD_DOWN] = Noesis::Key_GamepadDown;
            this->keys[VK_GAMEPAD_LEFT_THUMBSTICK_LEFT] = Noesis::Key_GamepadLeft;
            this->keys[VK_GAMEPAD_LEFT_THUMBSTICK_UP] = Noesis::Key_GamepadUp;
            this->keys[VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT] = Noesis::Key_GamepadRight;
            this->keys[VK_GAMEPAD_LEFT_THUMBSTICK_DOWN] = Noesis::Key_GamepadDown;
            this->keys[VK_GAMEPAD_A] = Noesis::Key_GamepadAccept;
            this->keys[VK_GAMEPAD_B] = Noesis::Key_GamepadCancel;
            this->keys[VK_GAMEPAD_MENU] = Noesis::Key_GamepadMenu;
            this->keys[VK_GAMEPAD_VIEW] = Noesis::Key_GamepadView;
        }

        Noesis::Key get_key(unsigned int vk)
        {
            return (vk < this->keys.size()) ? this->keys[vk] : Noesis::Key::Key_None;
        }

        bool valid(unsigned int vk)
        {
            return get_key(vk) != Noesis::Key_None;
        }

    private:
        std::array<Noesis::Key, 256> keys;

    } key_map;
}

bool ff::internal::ui::valid_key(unsigned int vk)
{
    return key_map.valid(vk);
}

Noesis::Key ff::internal::ui::get_key(unsigned int vk)
{
    return key_map.get_key(vk);
}

bool ff::internal::ui::valid_mouse_button(unsigned int vk)
{
    switch (vk)
    {
        case VK_LBUTTON:
        case VK_RBUTTON:
        case VK_MBUTTON:
        case VK_XBUTTON1:
        case VK_XBUTTON2:
            return true;

        default:
            return false;
    }
}

Noesis::MouseButton ff::internal::ui::get_mouse_button(unsigned int vk)
{
    switch (vk)
    {
        case VK_LBUTTON:
            return Noesis::MouseButton_Left;
        case VK_RBUTTON:
            return Noesis::MouseButton_Right;
        case VK_MBUTTON:
            return Noesis::MouseButton_Middle;
        case VK_XBUTTON1:
            return Noesis::MouseButton_XButton1;
        case VK_XBUTTON2:
            return Noesis::MouseButton_XButton2;
        default:
            assert(false);
            return Noesis::MouseButton_Count;
    }
}
