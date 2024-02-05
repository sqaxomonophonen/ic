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
#include "gl3w.h"

static bool show_window_tree = true;
static bool show_window_node = true;

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

						//if (i >= 2) ImGui::SameLine();

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
