#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "gl3w.h"

#include <SDL.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl4.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "util.h"

#include "iced.h"

static SDL_Window* window;

static bool fly = false;
static struct fly_state fly_state;
struct fly_state* get_fly_state(void)
{
	return fly ? &fly_state : NULL;
}
int fly_anchor_mx, fly_anchor_my;
void fly_enable(bool enable)
{
	if (enable) {
		memset(&fly_state, 0, sizeof fly_state);
		SDL_GetMouseState(&fly_anchor_mx, &fly_anchor_my);
	}
	SDL_SetRelativeMouseMode(enable ? SDL_TRUE : SDL_FALSE);
	if (!enable) {
		SDL_WarpMouseInWindow(window, fly_anchor_mx, fly_anchor_my);
	}
	fly = enable;
}


int main(int argc, char** argv)
{
	wchar_t* program = Py_DecodeLocale(argv[0], NULL);
	if (program == NULL) {
		fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
		exit(1);
	}
	Py_SetProgramName(program);

	assert(SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO) == 0);
	atexit(SDL_Quit);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);

	window = SDL_CreateWindow(
		"ICed",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1920, 1080,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	assert(window != NULL);
	SDL_GLContext glctx = SDL_GL_CreateContext(window);

	SDL_GL_SetSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(window, glctx);
	ImGui_ImplOpenGL4_Init(); // also does OpenGL loading; OpenGL calls may segfault before this

	io.Fonts->AddFontFromFileTTF("Roboto-Regular.ttf", 18);
	io.Fonts->Build();

	GLint gl_major_version, gl_minor_version;
	glGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);
	glGetIntegerv(GL_MINOR_VERSION, &gl_minor_version);
	printf("OpenGL%d.%d / GLSL%s\n", gl_major_version, gl_minor_version, glGetString(GL_SHADING_LANGUAGE_VERSION));

	iced_init();

	int exiting = 0;
	while (!exiting) {
		SDL_Event ev;
		float fly_dx = 0;
		float fly_dy = 0;
		float fly_wheel = 0; // hehe
		bool fly_stop = false;
		while (SDL_PollEvent(&ev)) {
			if ((ev.type == SDL_QUIT) || (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)) {
				exiting = 1;
			} else {
				if (fly) {
					switch (ev.type) {
					case SDL_KEYDOWN: {
						int sym = ev.key.keysym.sym;
						if (sym == SDLK_ESCAPE) {
							fly_state.cancel = true;
							fly_stop = true;
						}
						} break;
					case SDL_MOUSEMOTION:
						fly_dx += ev.motion.xrel;
						fly_dy += ev.motion.yrel;
						break;
					case SDL_MOUSEWHEEL:
						fly_wheel += ev.wheel.y;
						break;
					case SDL_MOUSEBUTTONDOWN:
					case SDL_MOUSEBUTTONUP:
						fly_state.accept = true;
						fly_stop = true;
						break;
					}
				} else {
					ImGui_ImplSDL2_ProcessEvent(&ev);
				}
			}
		}

		{
			fly_state.dyaw = fly_dx;
			fly_state.dpitch = fly_dy;
			fly_state.dzoom = fly_wheel;
			int numkeys = -1;
			const Uint8* kst = SDL_GetKeyboardState(&numkeys);
			#define GRAB(KEY) \
			bool KEY = false; \
			{ \
				SDL_Scancode scancode = SDL_GetScancodeFromKey((#KEY)[0]); \
				KEY = 0 <= scancode && scancode < numkeys && !!kst[scancode]; \
			}
			GRAB(w);
			GRAB(a);
			GRAB(s);
			GRAB(d);
			#undef GRAB

			SDL_Keymod mod = SDL_GetModState();
			fly_state.speed = 0;
			const int mag = mod & KMOD_ALT ? 2 : 1;
			if (mod & KMOD_SHIFT) fly_state.speed += mag;
			if (mod & KMOD_CTRL) fly_state.speed += mag;

			if (w && !s) {
				fly_state.dforward = 1;
			} else if (s && !w) {
				fly_state.dforward = -1;
			}

			if (d && !a) {
				fly_state.dright = 1;
			} else if (a && !d) {
				fly_state.dright = -1;
			}
		}

		ImGui_ImplOpenGL4_NewFrame();
		ImGui_ImplSDL2_NewFrame(!fly);
		ImGui::NewFrame();

		iced_gui();

		ImGui::Render();

		iced_render();

		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui_ImplOpenGL4_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);

		if (fly_stop) {
			fly_enable(false);
		}
	}

	PyMem_RawFree(program);

	return EXIT_SUCCESS;
}

// quality of life wrappers; these are defined in imgui_internal.h which makes
// compilation take noticeably longer, so they are wrapped here where I care
// less about compile times.
void imgui_own_wheel(void) { ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY); }
