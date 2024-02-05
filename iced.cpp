#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "imgui.h"
#include "util.h"
#include "iced.h"
#include "stb_ds.h"
#include "gb_math.h"

static void check_shader(GLuint shader, GLenum type, int n_sources, const char** sources)
{
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_TRUE) return;

	GLint msglen;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &msglen);
	GLchar* msg = (GLchar*) malloc(msglen + 1);
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
			buf[n] = 0;
			error_in_line_number = atoi(buf);
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
				for (;;) {
					char ch = *p;
					if (ch == 0) {
						is_end_of_string = 1;
						break;
					} else if (ch == '\n') {
						p++;
						break;
					} else {
						p++;
					}
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


#if 0
static char* read_file(const char* path, size_t* out_size)
{
	FILE* f = fopen(path, "rb");
	if (f == NULL) return NULL;
	assert(fseek(f, 0, SEEK_END) == 0);
	long sz = ftell(f);
	assert(fseek(f, 0, SEEK_SET) == 0);
	char* p = (char*)malloc(sz+1);
	assert(fread(p, sz, 1, f) == 1);
	assert(fclose(f) == 0);
	p[sz] = 0;
	if (out_size != NULL) *out_size = sz;
	return p;
}
#endif

static struct globals {
	bool lua_error;
	char lua_error_message[1<<12];
	lua_State* L;
	char* watch_paths_arr;
	struct timespec last_load_time;
	double duration_load;
	double duration_call;
} g;

static void watch_file(const char* path)
{
	const size_t n = strlen(path)+1;
	char* d = arraddnptr(g.watch_paths_arr, n);
	memcpy(d, path, n);
}

static inline void dump_timespec(struct timespec* ts)
{
	printf("[%ld.%ld]", ts->tv_sec, ts->tv_nsec);
}

static inline int timespec_compar(const struct timespec* ta, const struct timespec* tb)
{
	if (ta->tv_sec > tb->tv_sec) {
		return 1;
	} else if (ta->tv_sec < tb->tv_sec) {
		return -1;
	} else if (ta->tv_nsec > tb->tv_nsec) {
		return 1;
	} else if (ta->tv_nsec < tb->tv_nsec) {
		return -1;
	} else {
		return 0;
	}
}

static int lc_watch_file(lua_State* L)
{
	const int n = lua_gettop(L);
	if (n >= 1) watch_file(lua_tostring(L, 1));
	return 0;
}

static int l_errhandler(lua_State *L)
{
	// "stolen" from `msghandler()` in `lua.c`
	const char *msg = lua_tostring(L, 1);
	if (msg == NULL) {
		if (luaL_callmeta(L, 1, "__tostring") &&  lua_type(L, -1) == LUA_TSTRING) {
			return 1;
		}
	} else {
		msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
	}
	luaL_traceback(L, L, msg, 1);
	return 1;
}

static struct timespec timer_begin(void)
{
	struct timespec t0;
	assert(clock_gettime(CLOCK_MONOTONIC, &t0) == 0);
	return t0;
}

static double timer_end(struct timespec t0)
{
	struct timespec t1;
	assert(clock_gettime(CLOCK_MONOTONIC, &t1) == 0);
	return
		((double)(t1.tv_sec) - (double)(t0.tv_sec)) +
		1e-9 * ((double)(t1.tv_nsec) - (double)(t0.tv_nsec));
}

static int ecall(lua_State *L, int narg, int nres)
{
	// "stolen" from `docall()` in `lua.c`
	struct timespec t0 = timer_begin();
	int base = lua_gettop(L) - narg;
	lua_pushcfunction(L, l_errhandler);
	lua_insert(L, base);
	int e = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);
	g.duration_call = timer_end(t0);
	return e;
}

static int edofile(lua_State *L, const char* path)
{
	struct timespec t0 = timer_begin();
	int e;
	e = luaL_loadfile(L, path);
	if (e != 0) return e;
	e = ecall(L, 0, LUA_MULTRET);
	g.duration_load = timer_end(t0);
	return e;
}


static void handle_lua_error(void)
{
	g.lua_error = true;
	lua_State* L = g.L;
	const char* err = lua_tostring(L, -1);
	fprintf(stderr, "LUA ERROR: %s\n", err);
	snprintf(g.lua_error_message, sizeof g.lua_error_message, "%s", err);
	lua_pop(L, 1);
}

static void l_load(lua_State* L, const char* path)
{
	watch_file(path);
	if (g.lua_error) return;
	int e = edofile(L, path);
	if (e != 0) {
		handle_lua_error();
		return;
	}
}

static void lua_reload(void)
{
	assert(clock_gettime(CLOCK_REALTIME, &g.last_load_time) == 0);
	//dump_timespec(&g.last_load_time);

	if (g.L != NULL) {
		lua_close(g.L);
		g.L = NULL;
	}
	g.lua_error = false;
	arrsetlen(g.watch_paths_arr, 0);
	g.duration_load = 0;
	g.duration_call = 0;

	lua_State* L = g.L = luaL_newstate();
	luaL_openlibs(L);

	lua_pushcfunction(L, lc_watch_file);
	lua_setglobal(L, "lc_watch_file");

	l_load(L, "lib.lua");
	l_load(L, "world.lua");
}

static void check_for_reload(void)
{
	const char* p0 = g.watch_paths_arr;
	const char* p1 = p0 + arrlen(p0);
	const char* p = p0;
	bool reload = false;
	while (p < p1) {
		const size_t n = strlen(p);
		struct stat st;
		if (stat(p, &st) == 0) {
			if (timespec_compar(&st.st_mtim, &g.last_load_time) > 0) {
				reload = true;
				break;
			}
		}
		p += (n+1);
	}
	if (reload) lua_reload();
}

#define IS_Q0 "(gl_VertexID == 0 || gl_VertexID == 3)"
#define IS_Q1 "(gl_VertexID == 1)"
#define IS_Q2 "(gl_VertexID == 2 || gl_VertexID == 4)"
#define IS_Q3 "(gl_VertexID == 5)"

void iced_init(void)
{
	lua_reload();
}

struct view {
	const char* name;
	int dim;
	// TODO GL resources...? program? and?
};

struct view_window {
	const char* view_name;
	const char* window_title;
	int sequence;

	bool autoload;
	int pixel_size;

	bool gl_initialized;
	int fb_width, fb_height;
	GLuint framebuffer;
	GLuint texture;
	ImVec2 canvas_size;

	gbVec3 origin;
	float fov;
	float pitch;
	float yaw;
	int width;
	int height;
	// TODO GL resources: texture/framebuffer? and?
};

static struct view* view_arr;
static struct view_window* view_window_arr;

static bool window_view(struct view_window* w)
{
	bool show = true;
	if (ImGui::Begin(w->window_title, &show)) {
		if (ImGui::Button("Fly")) {
			// TODO wasd+mlock flymode
		}

		ImGui::SameLine();
		ImGui::Checkbox("Autoload", &w->autoload);

		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		ImGui::Combo("Px", &w->pixel_size, "1x" "\x0" "2x" "\x0" "3x" "\x0" "4x" "\x0\x0");

		ImGui::SameLine();
		if (ImGui::Button("Clone")) {
			// TODO new window, same view
		}

		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		ImVec2 canvas_size = ImGui::GetContentRegionAvail();
		if (canvas_size.x > 0 && canvas_size.y > 0) {
			const ImVec2 p1 = ImVec2(
				p0.x + canvas_size.x,
				p0.y + canvas_size.y);
			ImGui::InvisibleButton("canvas", canvas_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
			w->canvas_size = canvas_size;

			if (w->gl_initialized) {
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				draw_list->AddImage((void*)(intptr_t)w->texture, p0, p1);
			}
		}
	}
	ImGui::End();
	return show;
}

static void view_window_close(struct view_window* w)
{
	free((void*)w->view_name);
	free((void*)w->window_title);
	// TODO GL resources
}

static void lua_api_error(const char* msg)
{
	g.lua_error = true;
	snprintf(g.lua_error_message, sizeof g.lua_error_message, "[API ERROR] %s", msg);
}

static char* cstrdup(const char* s)
{
	const size_t n = strlen(s)+1;
	char* s2 = (char*)malloc(n);
	memcpy(s2, s, n);
	return s2;
}

static void open_view_window(struct view* view)
{
	int sequence = 0;
	{
		const int n = arrlen(view_window_arr);
		for (int i = 0; i < n; i++) {
			struct view_window* vw2 = &view_window_arr[i];
			if (vw2->sequence >= sequence && strcmp(vw2->view_name, view->name) == 0) {
				sequence = vw2->sequence + 1;
			}
		}
	}

	char wt[1<<10];
	snprintf(wt, sizeof wt, "[%dD] %s /%d", view->dim, view->name, sequence);
	struct view_window vw = {
		.view_name = cstrdup(view->name),
		.window_title = cstrdup(wt),
		.sequence = sequence,
		.autoload = true,
	};
	arrput(view_window_arr, vw);
}

static void open_view(lua_State* L)
{
	lua_getfield(L, -1, "name");
	const char* name = lua_tostring(L, -1);
	const int n = arrlen(view_arr);
	for (int i = 0; i < n; i++) {
		struct view* view = &view_arr[i];
		if (strcmp(view->name, name) == 0) {
			lua_pop(L, 1);
			open_view_window(view);
			return;
		}
	}

	struct view view = {0};
	view.name = cstrdup(name);
	lua_pop(L, 1);

	lua_getfield(L, -1, "dim");
	view.dim = lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getglobal(L, "ll_view_run");
	lua_pushvalue(L, -2);
	int e = ecall(L, 1, 1);
	if (e != 0) {
		handle_lua_error();
		return;
	}

	const char* glsl_src = lua_tostring(L, -1);
	// TODO

	lua_pop(L, 1);

	arrput(view_arr, view);

	open_view_window(&view);
}


static void window_main(void)
{
	static bool show_main = true;
	if (show_main) {
		if (ImGui::Begin("Main", &show_main)) {
			lua_State* L = g.L;

			if (g.lua_error) {
				ImGui::SeparatorText("Lua Error");
				ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "%s", g.lua_error_message);
			}

			ImGui::SeparatorText("Status");
			ImGui::Text("Load: %fs\nCall: %fs\nStack: %d",
				g.duration_load,
				g.duration_call,
				lua_gettop(L));


			if (L != NULL && !g.lua_error) {
				lua_getglobal(L, "ll_views");
				if (!lua_istable(L, -1)) {
					lua_api_error("no global table named \"ll_views\"");
				} else {
					unsigned n = lua_rawlen(L, -1);
					bool show = true;
					for (unsigned i = 1; i <= n; i++) {
						if (i == 1) ImGui::SeparatorText("Views");

						lua_rawgeti(L, -1, i);

						lua_getfield(L, -1, "header");
						if (!lua_isnil(L, -1)) {
							const char* header = lua_tostring(L, -1);
							show = ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_None);
							lua_pop(L, 2);
							continue;
						} else {
							lua_pop(L, 1);
						}

						if (!show) {
							lua_pop(L, 1);
							continue;
						}

						lua_getfield(L, -1, "dim");
						const int dim = lua_tointeger(L, -1);
						if (dim != 2 && dim != 3) {
							lua_api_error("view dimension is not 2 or 3");
							break;
						}
						lua_pop(L, 1);

						lua_getfield(L, -1, "name");
						const char* name = lua_tostring(L, -1);
						char buf[1<<10];
						snprintf(buf, sizeof buf, "[%dD] %s##view%d", dim, name, i);
						lua_pop(L, 1);

						if (ImGui::Button(buf)) open_view(L);

						lua_pop(L, 1);
					}
				}
				lua_pop(L, 1);
			}
		}
		ImGui::End();
	}
}

void iced_gui(void)
{
	check_for_reload();
	window_main();

	for (int i = 0; i < arrlen(view_window_arr); i++) {
		struct view_window* vw = &view_window_arr[i];
		if (!window_view(vw)) {
			view_window_close(vw);
			arrdel(view_window_arr, i);
			i--;
		}
	}

}

static inline float fremap(float i, float i0, float i1, float o0, float o1)
{
	return o0 + ((i - i0) / (i1 - i0)) * (o1 - o0);
}

void iced_render(void)
{
	for (int i = 0; i < arrlen(view_window_arr); i++) {
		struct view_window* vw = &view_window_arr[i];
		const ImVec2 size = vw->canvas_size;
		const int px = vw->pixel_size+1;
		const int fb_width = (int)size.x / px;
		const int fb_height = (int)size.y / px;

		if (fb_width <= 0 || fb_height <= 0) continue;

		bool do_render = false; // FIXME true if stuff has otherwise changed

		if (!vw->gl_initialized) {
			glGenFramebuffers(1, &vw->framebuffer); CHKGL;
			glGenTextures(1, &vw->texture); CHKGL;

			vw->gl_initialized = true;
			vw->fb_width = -1;
			vw->fb_height = -1;
		}

		if (fb_width != vw->fb_width || fb_height != vw->fb_height) {
			glBindTexture(GL_TEXTURE_2D, vw->texture); CHKGL;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); CHKGL;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); CHKGL;
			glTexImage2D(GL_TEXTURE_2D, /*level=*/0, GL_RGB, fb_width, fb_height, /*border=*/0, GL_RGB, GL_UNSIGNED_BYTE, NULL); CHKGL;

			#if 1
			{
				// upload debug texture
				const int bpp = 3;
				const int row_size0 = fb_width * bpp;
				const int row_size = ((row_size0+3) >> 2) << 2;
				const int stride = row_size - row_size0;
				unsigned char* pixels = (unsigned char*)malloc(row_size*fb_height);
				unsigned char* p = pixels;
				for (int y = 0; y < fb_height; y++) {
					for (int x = 0; x < fb_width; x++) {
						int chk = ((x>>3) ^ (y>>3)) & 1;
						p[0] = chk ? 255 : 0;
						p[1] = chk ? 255 : 0;
						p[2] = chk ?   0 : 255;
						p += bpp;
					}
					p += stride;
				}
				glTexSubImage2D(GL_TEXTURE_2D, /*level=*/0, /*xOffset=*/0, /*yOffset=*/0, fb_width, fb_height, GL_RGB, GL_UNSIGNED_BYTE, pixels); CHKGL;
				free(pixels);
			}
			#endif

			glBindTexture(GL_TEXTURE_2D, 0); CHKGL;
			vw->fb_width = fb_width;
			vw->fb_height = fb_height;
			do_render = true;
		}

		#if 0
		if (do_render) {
			glBindFramebuffer(GL_FRAMEBUFFER, vw->framebuffer); CHKGL;
			glViewport(0, 0, fb_width, fb_height);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vw->texture, /*level=*/0); CHKGL;
			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

			//glDrawArrays(GL_TRIANGLES, 0, 6); CHKGL;
			glBindFramebuffer(GL_FRAMEBUFFER, 0); CHKGL;
		}
		#endif
	}
}
