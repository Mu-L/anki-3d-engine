// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Window/NativeWindow.h>
#include <SDL3/SDL.h>

namespace anki {

/// Native window implementation for SDL
class NativeWindowSdl : public NativeWindow
{
public:
	SDL_Window* m_sdlWindow = nullptr;

	~NativeWindowSdl();

	Error initSdl(const NativeWindowInitInfo& init);

private:
	static constexpr U32 kInitSubsystems = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD;
};

} // end namespace anki
