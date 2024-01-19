#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "gl3w.h"

#include <SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl4.h"

#include "util.h"

struct globals {
	
} g;

#define CHKGL { GLenum xx_GLERR = glGetError(); if (xx_GLERR != GL_NO_ERROR) { fprintf(stderr, "OPENGL ERROR %d in %s:%d", xx_GLERR, __FILE__, __LINE__); abort(); } }

static void check_shader(GLuint shader, GLenum type, int n_sources, const char** sources)
{
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_TRUE) return;

	GLint msglen;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &msglen);                                  GLchar* msg = (GLchar*) malloc(msglen + 1);
	assert(msg != NULL);
	glGetShaderInfoLog(shader, msglen, NULL, msg);
	const char* stype =
	 	type == GL_COMPUTE_SHADER ? "COMPUTE" :
	 	type == GL_VERTEX_SHADER ? "VERTEX" :
		type == GL_FRAGMENT_SHADER ? "FRAGMENT" :
		"???";

	// attempt to parse "0:<linenumber>" in error message
	int error_in_line_number = 0;
	if (strlen(msg) >= 3 && msg[0] == '0' && msg[1] == ':' && '0' <= msg[2] && msg[2] <= '9') {
		const char* p0 = msg+2;
		const char* p1 = p0+1;
		while ('0' <= *p1 && *p1 <= '9') p1++;
		char buf[32];
		const int n = p1-p0;
		if (n < ARRAY_LENGTH(buf)) {
			memcpy(buf, p0, n);
			buf[n] = 0;                                                                          error_in_line_number = atoi(buf);
		}
	}

	fprintf(stderr, "%s GLSL COMPILE ERROR: %s in:\n", stype, msg);
	if (error_in_line_number > 0) {
		char line_buffer[1<<14];
		int line_number = 1;
		for (int pi = 0; pi < n_sources; pi++) {
			const char* p = sources[pi];
			int is_end_of_string = 0;
			while (!is_end_of_string)  {
				const char* p0 = p;
				for (;;) {                                                                                   char ch = *p;
					if (ch == 0) {
						is_end_of_string = 1;
						break;
					} else if (ch == '\n') {                                                                     p++;
						break;
					} else {
						p++;                                                                         }
				}
				if (p > p0) {
					size_t n = (p-1) - p0;
					if (n >= sizeof(line_buffer)) n = sizeof(line_buffer)-1;
					memcpy(line_buffer, p0, n);
					line_buffer[n] = 0;
					fprintf(stderr, "(%.4d)  %s\n", line_number, line_buffer);
				}
				if (line_number == error_in_line_number) {
					fprintf(stderr, "~^~^~^~ ERROR ~^~^~^~\n");
				}
				line_number++;
			}
			line_number--;
		}
	} else {
		for (int i = 0; i < n_sources; i++) {
			fprintf(stderr, "src[%d]: %s\n", i, sources[i]);
		}
	}
	fprintf(stderr, "shader compilation failed\n");
	abort();
}

static void check_program(GLint program)
{
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_TRUE) return;
	GLint msglen;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &msglen);
	GLchar* msg = (GLchar*) malloc(msglen + 1);
	glGetProgramInfoLog(program, msglen, NULL, msg);
	fprintf(stderr, "shader link error: %s", msg);
	abort();
}


static GLuint mk_shader(GLenum type, int n_sources, const char** sources)
{
	GLuint shader = glCreateShader(type); CHKGL;
	glShaderSource(shader, n_sources, sources, NULL); CHKGL;
	glCompileShader(shader); CHKGL;
	check_shader(shader, type, n_sources, sources);
	return shader;
}

static GLuint mk_compute_program(int n_sources, const char** sources)
{
	GLuint shader = mk_shader(GL_COMPUTE_SHADER, n_sources, sources);
	GLuint program = glCreateProgram(); CHKGL;
	glAttachShader(program, shader); CHKGL;
	glLinkProgram(program); CHKGL;
	check_program(program);

	// when we have a program the shader is no longer needed
	glDeleteShader(shader); CHKGL;

	return program;
}

static GLuint mk_render_program(int n_vertex_sources, int n_fragment_sources, const char** sources)
{
	const char** vertex_sources = sources;
	const char** fragment_sources = sources +  n_vertex_sources;
	GLuint vertex_shader = mk_shader(GL_VERTEX_SHADER, n_vertex_sources, vertex_sources);
	GLuint fragment_shader = mk_shader(GL_FRAGMENT_SHADER, n_fragment_sources, fragment_sources);
	GLuint program = glCreateProgram(); CHKGL;
	glAttachShader(program, vertex_shader); CHKGL;
	glAttachShader(program, fragment_shader); CHKGL;
	glLinkProgram(program); CHKGL;
	check_program(program);

	// when we have a program the shaders are no longer needed
	glDeleteShader(vertex_shader); CHKGL;
	glDeleteShader(fragment_shader); CHKGL;

	return program;
}


static void gl_init(void)
{
	//mk_compute_program(sources);
	{
		const char* sources[] = {

		"#version 460\n"
		"\n"
		"layout (location = 0) in vec3 a_pos;\n"
		"layout (location = 1) in vec4 a_color;\n"
		"\n"
		"layout (location = 0) uniform mat4 u_tx;\n"
		"\n"
		"out vec4 v_color;\n"
		"\n"
		"void main()\n"
		"{\n"
		"	v_color = a_color;\n"
		"	gl_Position = u_tx * vec4(a_pos,1);\n"
		"}\n"

		,

		"#version 460\n"
		"\n"
		"in vec4 v_color;\n"
		"\n"
		"layout (location = 0) out vec4 frag_color;\n"
		"\n"
		"void main()\n"
		"{\n"
		"	frag_color = v_color;\n"
		"}\n"

		};

		mk_render_program(1, 1, sources);
	}
}

static void begin_vg(void)
{
}

static void end_vg(void)
{
}

//static void vg_line



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

	GLint gl_major_version, gl_minor_version;
	glGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);
	glGetIntegerv(GL_MINOR_VERSION, &gl_minor_version);
	printf("OpenGL%d.%d / GLSL%s\n", gl_major_version, gl_minor_version, glGetString(GL_SHADING_LANGUAGE_VERSION));

	gl_init();

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

		if (ImGui::Begin("XXX")) {
			ImGui::Button("test");
			ImGui::End();
		}

		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);




		ImGui_ImplOpenGL4_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	return EXIT_SUCCESS;
}
