// Run the native button-number mapping through SDL using a virtual 16-button pad.
#include "../platform/3ds/original/cat3ds_pad.h"
#include <SDL3/SDL.h>
#include <cassert>

int main()
{
    assert(SDL_Init(SDL_INIT_GAMEPAD));
    SDL_VirtualJoystickDesc desc{};
    SDL_INIT_INTERFACE(&desc);
    desc.name = "Catacomb synthetic 3DS";
    desc.nbuttons = 16;
    desc.naxes = 4;
    auto id = SDL_AttachVirtualJoystick(&desc);
    assert(id && SDL_SetGamepadMapping(id, CAT3DS_SDL_MAPPING));
    auto *pad = SDL_OpenGamepad(id);
    assert(pad);
    auto *joystick = SDL_GetGamepadJoystick(pad);
    struct Binding { int index; SDL_GamepadButton logical; };
    const Binding bindings[] = {
        {0,SDL_GAMEPAD_BUTTON_SOUTH}, {1,SDL_GAMEPAD_BUTTON_EAST},
        {10,SDL_GAMEPAD_BUTTON_WEST}, {11,SDL_GAMEPAD_BUTTON_NORTH},
        {2,SDL_GAMEPAD_BUTTON_BACK}, {3,SDL_GAMEPAD_BUTTON_START},
        {4,SDL_GAMEPAD_BUTTON_DPAD_RIGHT}, {5,SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {6,SDL_GAMEPAD_BUTTON_DPAD_UP}, {7,SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {8,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}, {9,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER}
    };
    for (auto binding : bindings) {
        assert(SDL_SetJoystickVirtualButton(joystick,binding.index,true));
        SDL_UpdateJoysticks();
        assert(SDL_GetGamepadButton(pad,binding.logical));
        assert(SDL_SetJoystickVirtualButton(joystick,binding.index,false));
        SDL_UpdateJoysticks();
        assert(!SDL_GetGamepadButton(pad,binding.logical));
    }
    SDL_CloseGamepad(pad);
    assert(SDL_DetachVirtualJoystick(id));
    SDL_Quit();
}
