#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "imgui.h"
#include "util.h"
#include "iced.h"
#include "stb_ds.h"
#include "gb_math.h"

static uint64_t _serial;
static uint64_t next_serial(void)
{
	return ++_serial;
}

bool has_glsl_error;
static char glsl_error[1<<13];

static char* cstrdup(const char* s)
{
	const size_t n = strlen(s)+1;
	char* s2 = (char*)malloc(n);
	memcpy(s2, s, n);
	return s2;
}

static void check_shader(GLuint shader, GLenum type, int n_sources, const char** sources)
{
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_TRUE) {
		has_glsl_error = false;
		return;
	}

	has_glsl_error = true;

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

	char* pe = glsl_error;
	char* pe1 = glsl_error + sizeof glsl_error;

	pe += snprintf(pe, pe1-pe, "%s GLSL COMPILE ERROR: %s in:\n", stype, msg);
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
					pe += snprintf(pe, pe1-pe, "(%.4d)  %s\n", line_number, line_buffer);
				}
				if (line_number == error_in_line_number) {
					pe += snprintf(pe, pe1-pe, "~^~^~^~ ERROR ~^~^~^~\n");
				}
				line_number++;
			}
			line_number--;
		}
	} else {
		for (int i = 0; i < n_sources; i++) {
			pe += snprintf(pe, pe1-pe, "src[%d]: %s\n", i, sources[i]);
		}
	}
	pe += snprintf(pe, pe1-pe, "shader compilation failed\n");
}

static void check_program(GLint program)
{
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_TRUE) {
		has_glsl_error = false;
		return;
	}
	has_glsl_error = true;
	GLint msglen;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &msglen);
	GLchar* msg = (GLchar*) malloc(msglen + 1);
	glGetProgramInfoLog(program, msglen, NULL, msg);
	snprintf(glsl_error, sizeof glsl_error, "shader link error: %s", msg);
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
	if (has_glsl_error) {
		glDeleteShader(shader); CHKGL;
		return 0;
	}
	GLuint program = glCreateProgram(); CHKGL;
	glAttachShader(program, shader); CHKGL;
	glLinkProgram(program); CHKGL;
	check_program(program);
	if (has_glsl_error) {
		glDeleteProgram(program); CHKGL;
	}

	// when we have a program the shader is no longer needed
	glDeleteShader(shader); CHKGL;

	return program;
}

static GLuint mk_render_program(int n_vertex_sources, int n_fragment_sources, const char** sources)
{
	const char** vertex_sources = sources;
	const char** fragment_sources = sources +  n_vertex_sources;
	GLuint vertex_shader = mk_shader(GL_VERTEX_SHADER, n_vertex_sources, vertex_sources);
	if (has_glsl_error) {
		glDeleteShader(vertex_shader); CHKGL;
		return 0;
	}
	GLuint fragment_shader = mk_shader(GL_FRAGMENT_SHADER, n_fragment_sources, fragment_sources);
	if (has_glsl_error) {
		glDeleteShader(vertex_shader); CHKGL;
		glDeleteShader(fragment_shader); CHKGL;
		return 0;
	}
	GLuint program = glCreateProgram(); CHKGL;
	glAttachShader(program, vertex_shader); CHKGL;
	glAttachShader(program, fragment_shader); CHKGL;
	glLinkProgram(program); CHKGL;
	check_program(program);
	if (has_glsl_error) {
		glDeleteProgram(program); CHKGL;
	}

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
	bool python_initialized;
	bool python_do_reinitialize;
	PyObject* python_world_module;
	bool has_error;
	char error_message[1<<14];
	char* watch_paths_arr;
	struct timespec last_load_time;
	double duration_load;
	double duration_call;
	GLuint vao0;
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

struct view {
	const char* name;
	int dim;
	GLuint prg0;
	uint64_t serial;
};

struct view_window {
	bool dispose;

	const char* view_name;
	const char* window_title;
	int sequence;

	int pixel_size;

	bool gl_initialized;
	int fb_width, fb_height;
	GLuint framebuffer;
	GLuint texture;
	ImVec2 canvas_size;

	uint64_t serial;
	uint64_t seen_serial;

	struct {
		gbVec2 origin;
		float scale;
		bool is_panning;
	} d2;

	struct {
		gbVec3 origin;
		float fov;
		float pitch;
		float yaw;
	} d3;
};

static struct view* view_arr;
static struct view_window* view_window_arr;


#define IS_Q0 "(gl_VertexID == 0 || gl_VertexID == 3)"
#define IS_Q1 "(gl_VertexID == 1)"
#define IS_Q2 "(gl_VertexID == 2 || gl_VertexID == 4)"
#define IS_Q3 "(gl_VertexID == 5)"

static void reload_view(struct view* view)
{
	const char* glsl_src = "XXX"; // XXX
	if (view->dim == 2) {
		const char* sources[] = {

			// vertex
			"#version 460\n"
			"\n"
			"layout (location = 0) uniform vec2 u_p0;\n"
			"layout (location = 1) uniform vec2 u_p1;\n"
			"\n"
			"out vec2 v_pos;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	vec2 c;\n"
			"	if (" IS_Q0 ") {\n"
			"		c = vec2(-1.0, -1.0);\n"
			"		v_pos = vec2(u_p0.x, u_p0.y);\n"
			"	} else if (" IS_Q1 ") {\n"
			"		c = vec2( 1.0, -1.0);\n"
			"		v_pos = vec2(u_p1.x, u_p0.y);\n"
			"	} else if (" IS_Q2 ") {\n"
			"		c = vec2( 1.0,  1.0);\n"
			"		v_pos = vec2(u_p1.x, u_p1.y);\n"
			"	} else if (" IS_Q3 ") {\n"
			"		c = vec2(-1.0,  1.0);\n"
			"		v_pos = vec2(u_p0.x, u_p1.y);\n"
			"	}\n"
			"	gl_Position = vec4(c,0.0,1.0);\n"
			"}\n"
			,

			// fragment
			"#version 460\n"
			"\n"
			,
			glsl_src
			,
			"\n"
			"in vec2 v_pos;\n"
			"\n"
			"layout (location = 0) out vec4 frag_color;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	float d = map(v_pos);\n"
			"	float m = d > 0.0 ? min(1.0, 0.6+d*0.1) : 1.0;\n"
			"	float m2 = max(0.0, 1.0 - abs(d*0.03));\n"
			"	m2 = m2*m2*m2;\n"
			"	vec3 c = (d < 0.0) ? vec3(1.0, 0.9, 0.8) : vec3(0.2*m2, 0.3*m2, 0.4);\n"
			"	vec3 a0 = (d < 0.0) ? vec3(-0.0, -0.1, -0.2) : vec3(0.02, -0.03, -0.03);\n"
			"	float cc = cos(d*3.0);\n"
			"	cc = cc*cc*cc*cc*cc*cc*cc*cc*cc;\n"
			"	c += cc * a0;\n"
			"	frag_color = vec4(m*c, 1.0);\n"
			"}\n"
		};

		GLuint new_prg = mk_render_program(1, 3, sources);
		if (has_glsl_error) {
			snprintf(g.error_message, sizeof g.error_message, "[GLSL ERROR] %s", glsl_error);
			g.has_error = true;
		} else {
			if (view->prg0) {
				glDeleteProgram(view->prg0); CHKGL;
			}
			view->prg0 = new_prg;
			view->serial = next_serial();
		}

	} else if (view->dim == 3) {
		assert(!"TODO 3D"); // XXX
	}
}

static void reload_script(void)
{
	assert(clock_gettime(CLOCK_REALTIME, &g.last_load_time) == 0);
	//dump_timespec(&g.last_load_time);

	g.has_error = false;
	g.duration_load = 0;
	g.duration_call = 0;

	const bool must_init = !g.python_initialized || g.python_do_reinitialize;
	g.python_do_reinitialize = false;
	struct timespec t0 = timer_begin();
	if (must_init && g.python_initialized) {
		assert(!(Py_FinalizeEx() < 0));
		g.python_initialized = false;
	}

	if (must_init) {
		assert(!g.python_initialized);
		Py_Initialize();
		g.python_initialized = true;
		PyRun_SimpleString(
			"import sys\n"
			"sys.path.insert(0,'')\n" // ensure local modules can be imported
		);
	}

	assert(g.python_initialized);

	if (must_init) {
		PyObject* pn = PyUnicode_DecodeFSDefault("world");
		g.python_world_module = PyImport_Import(pn);
		Py_DECREF(pn);
	} else {
		g.python_world_module = PyImport_ReloadModule(g.python_world_module);
	}
	if (g.python_world_module == NULL) {
		PyErr_Print();
		fprintf(stderr, "ERROR: import failed\n");
		g.python_initialized = false;
	}

	PyObject* pfn = PyObject_GetAttrString(g.python_world_module, "watchlist");
	if (pfn != NULL) {
		if (PyCallable_Check(pfn)) {
			PyObject* pr = PyObject_CallObject(pfn, NULL);
			if (pr != NULL) {
				arrsetlen(g.watch_paths_arr, 0);
				PyObject* it = PyObject_GetIter(pr);
				PyObject* item;
				while ((item = PyIter_Next(it)) != NULL) {
					PyObject* item_str = PyUnicode_AsEncodedString(item, "utf-8", "Error ~");
					if (item_str != NULL) {
						char* item_cstr = PyBytes_AS_STRING(item_str);
						watch_file(item_cstr);
						Py_DECREF(item_str);
					}

					Py_DECREF(item);
				}
				Py_DECREF(it);
				Py_DECREF(pr);
			}
		}
		Py_DECREF(pfn);
	}

	g.duration_load = timer_end(t0);

	for (int i = 0; i < arrlen(view_arr); i++) {
		struct view* view = &view_arr[i];
		reload_view(view);
	}
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
	if (!reload) return;

	reload_script();
}

void iced_init(void)
{
	reload_script();
	glGenVertexArrays(1, &g.vao0); CHKGL;
}

static struct view* get_view_window_view(struct view_window* vw)
{
	const int n = arrlen(view_arr);
	for (int i = 0; i < n; i++) {
		struct view* view = &view_arr[i];
		if (strcmp(view->name, vw->view_name) == 0) {
			return view;
		}
	}
	assert(!"no view?!");
}

static void window_view(struct view_window* vw)
{
	struct view* view = get_view_window_view(vw);
	ImGuiIO& io = ImGui::GetIO();
	bool show = true;
	if (ImGui::Begin(vw->window_title, &show)) {
		const int dim = view->dim;

		if (dim == 3) {
			if (ImGui::Button("Fly")) {
				fly_enable(true);
			}
			ImGui::SameLine();
		}

		ImGui::SetNextItemWidth(70);
		ImGui::Combo("Px", &vw->pixel_size, "1x" "\x0" "2x" "\x0" "3x" "\x0" "4x" "\x0\x0");

		ImGui::SameLine();
		if (ImGui::Button("Clone")) {
			// TODO new window, same view
		}

		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		const ImVec2 canvas_size = ImGui::GetContentRegionAvail();

		const ImVec2 mousepos = io.MousePos;
		const bool mousepos_avail = mousepos.x != -FLT_MAX;
		const float mx = mousepos.x - (p0.x + canvas_size.x * 0.5f);
		const float my = mousepos.y - (p0.y + canvas_size.y * 0.5f);

		if (dim == 2) {
			if (mousepos_avail) {
				ImGui::SameLine();
				const float x = vw->d2.origin.x + mx * vw->d2.scale;
				const float y = vw->d2.origin.y + my * vw->d2.scale;
				ImGui::Text("[%.3f,%.3f]", x, y);
			}
		}

		if (canvas_size.x > 0 && canvas_size.y > 0) {
			vw->canvas_size = canvas_size;
			const int px = vw->pixel_size+1;
			const int adjw = (((int)canvas_size.x) / px) * px;
			const int adjh = (((int)canvas_size.y) / px) * px;
			const ImVec2 p1 = ImVec2(p0.x + adjw, p0.y + adjh);
			ImGui::InvisibleButton("canvas", canvas_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
			imgui_own_wheel();
			//const bool is_drag = ImGui::IsItemActive();
			const bool is_hover = ImGui::IsItemHovered();
			//const bool click_lmb = is_hover && ImGui::IsMouseClicked(0);
			const bool click_rmb = is_hover && ImGui::IsMouseClicked(1);
			//const bool doubleclick_rmb = is_hover && ImGui::IsMouseDoubleClicked(1);
			const float mw = io.MouseWheel;

			if (dim == 2) {
				if (click_rmb) {
					vw->d2.is_panning = true;
				}

				if (vw->d2.is_panning) {
					ImVec2 d = io.MouseDelta;
					if (d.x != 0 || d.y != 0) {
						const float s = vw->d2.scale;
						vw->d2.origin.x -= d.x*s;
						vw->d2.origin.y -= d.y*s;
						vw->serial = next_serial();
					}

					if (ImGui::IsMouseReleased(1)) {
						vw->d2.is_panning = false;
					}
				}

				if (is_hover && mw != 0) {
					const float sc0 = vw->d2.scale;
					vw->d2.scale *= powf(1.02f, -mw);
					const float sc1 = vw->d2.scale;
					vw->d2.origin.x += (mx * sc0) - (mx * sc1);
					vw->d2.origin.y += (my * sc0) - (my * sc1);
					vw->serial = next_serial();
				}
			}

			if (vw->gl_initialized && vw->texture) {
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				draw_list->AddImage((void*)(intptr_t)vw->texture, p0, p1);
			}
		}
	}
	ImGui::End();
	if (!show) vw->dispose = true;
}

static void view_window_free(struct view_window* vw)
{
	if (vw->gl_initialized) {
		glDeleteTextures(1, &vw->texture);
		glDeleteFramebuffers(1, &vw->framebuffer);
		vw->texture = 0;
		vw->framebuffer = 0;
		vw->gl_initialized = false;
	}
	free((void*)vw->view_name);
	free((void*)vw->window_title);
}

static void view_free(struct view* v)
{
	glDeleteProgram(v->prg0);
	free((void*)v->name);
}

static void lua_api_error(const char* msg)
{
	g.has_error = true;
	snprintf(g.error_message, sizeof g.error_message, "[API ERROR] %s", msg);
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
		.d2 = { .scale = 0.03, },
	};
	arrput(view_window_arr, vw);
}

static void window_main(void)
{
	static bool show_main = true;
	if (show_main) {
		if (ImGui::Begin("Main", &show_main)) {
			if (g.has_error) {
				ImGui::SeparatorText("Error");
				ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "%s", g.error_message);
			}

			ImGui::SeparatorText("Status");
			ImGui::Text("Load: %fs\nCall: %fs",
				g.duration_load,
				g.duration_call);
			if (ImGui::Button("Soft Reload")) {
				reload_script();
			}
			ImGui::SameLine();
			if (ImGui::Button("Hard")) {
				g.python_do_reinitialize = true;
				reload_script();
			}
		}
		ImGui::End();
	}
}

void iced_gui(void)
{
	for (int i0 = 0; i0 < arrlen(view_window_arr); i0++) {
		struct view_window* vw = &view_window_arr[i0];
		if (vw->dispose) {
			bool also_dispose_view = true;
			for (int i1 = 0; i1 < arrlen(view_window_arr); i1++) {
				if (i1 == i0) continue;
				struct view_window* vw1 = &view_window_arr[i1];
				if (strcmp(vw1->view_name, vw->view_name) == 0) {
					also_dispose_view = false;
					break;
				}
			}
			if (also_dispose_view) {
				bool disposed_view = false;
				for (int i1 = 0; i1 < arrlen(view_arr); i1++) {
					struct view* v = &view_arr[i1];
					if (strcmp(v->name, vw->view_name) == 0) {
						view_free(v);
						arrdel(view_arr, i1);
						disposed_view = true;
						break;
					}
				}
				assert(disposed_view);
			}
			view_window_free(vw);
			arrdel(view_window_arr, i0);
			i0--;
		}
	}
	check_for_reload();
	window_main();

	for (int i = 0; i < arrlen(view_window_arr); i++) {
		struct view_window* vw = &view_window_arr[i];
		window_view(vw);
	}
}

static inline float fremap(float i, float i0, float i1, float o0, float o1)
{
	return o0 + ((i - i0) / (i1 - i0)) * (o1 - o0);
}

void iced_render(void)
{
	const int n = arrlen(view_window_arr);
	for (int i = 0; i < n; i++) {
		struct view_window* vw = &view_window_arr[i];

		struct view* view = get_view_window_view(vw);

		const ImVec2 size = vw->canvas_size;
		const int px = vw->pixel_size+1;
		const int fb_width = (int)size.x / px;
		const int fb_height = (int)size.y / px;

		if (fb_width <= 0 || fb_height <= 0) continue;

		bool do_render = (view->serial > vw->seen_serial) || (vw->serial > vw->seen_serial);

		if (view->serial > vw->seen_serial) vw->seen_serial = view->serial;
		if (vw->serial > vw->seen_serial) vw->seen_serial = vw->serial;

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
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;
			glTexImage2D(GL_TEXTURE_2D, /*level=*/0, GL_RGB, fb_width, fb_height, /*border=*/0, GL_RGB, GL_UNSIGNED_BYTE, NULL); CHKGL;

			#if 0
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

		if (do_render) {
			glBindFramebuffer(GL_FRAMEBUFFER, vw->framebuffer); CHKGL;
			glViewport(0, 0, fb_width, fb_height);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vw->texture, /*level=*/0); CHKGL;
			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

			glUseProgram(view->prg0); CHKGL;
			if (view->dim == 2) {
				const gbVec2 o = vw->d2.origin;
				const float sc = vw->d2.scale * (float)px;
				const float dx = (float)fb_width * sc;
				const float dy = (float)fb_height * sc;
				const float x0 = o.x - dx*0.5;
				const float y0 = o.y - dy*0.5;
				const float x1 = o.x + dx*0.5;
				const float y1 = o.y + dy*0.5;
				glUniform2f(0, x0, y0);
				glUniform2f(1, x1, y1);
			} else if (view->dim == 3) {
				assert(!"TODO dim=3");
			} else {
				assert(!"bad");
			}

			glBindVertexArray(g.vao0); CHKGL;
			glDrawArrays(GL_TRIANGLES, 0, 6); CHKGL;
			glBindVertexArray(0); CHKGL;

			glBindFramebuffer(GL_FRAMEBUFFER, 0); CHKGL;
		}
	}
}
