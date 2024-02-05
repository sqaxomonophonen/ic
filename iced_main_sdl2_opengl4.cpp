#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "gl3w.h"

#include <SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl4.h"

#include "util.h"

#include "iced.h"

int main(int argc, char** argv)
{
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

	SDL_Window* window = SDL_CreateWindow(
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
		while (SDL_PollEvent(&ev)) {
			if ((ev.type == SDL_QUIT) || (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)) {
				exiting = 1;
			} else {
				ImGui_ImplSDL2_ProcessEvent(&ev);
			}
		}

		ImGui_ImplOpenGL4_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		iced_gui();

		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// TODO
		ImGui_ImplOpenGL4_RenderDrawData(ImGui::GetDrawData());

		iced_render();

		SDL_GL_SwapWindow(window);
	}

	return EXIT_SUCCESS;
}
