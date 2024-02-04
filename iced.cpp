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
	struct timespec tt;
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

static int l_watch_file(lua_State* L)
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

static int ecall(lua_State *L, int narg, int nres)
{
	// "stolen" from `docall()` in `lua.c`
	int base = lua_gettop(L) - narg;
	lua_pushcfunction(L, l_errhandler);
	lua_insert(L, base);
	int e = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);
	return e;
}

static int edofile(lua_State *L, const char* path)
{
	int e;
	e = luaL_loadfile(L, path);
	if (e != 0) return e;
	return ecall(L, 0, LUA_MULTRET);
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
	assert(clock_gettime(CLOCK_REALTIME, &g.tt) == 0);
	//dump_timespec(&g.tt);

	if (g.L != NULL) {
		lua_close(g.L);
		g.L = NULL;
	}
	g.lua_error = false;
	arrsetlen(g.watch_paths_arr, 0);

	lua_State* L = g.L = luaL_newstate();
	luaL_openlibs(L);

	lua_pushcfunction(L, l_watch_file);
	lua_setglobal(L, "watch_file");

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
			if (timespec_compar(&st.st_mtim, &g.tt) > 0) {
				reload = true;
				break;
			}
		}
		p += (n+1);
	}
	if (reload) {
		lua_reload();
	}
}

void iced_init(void)
{
	lua_reload();
}

static void lua_stuff(void)
{
	lua_State* L = g.L;
	if (L != NULL) {
		lua_getglobal(L, "view_names");
		if (lua_istable(L, -1)) {
			unsigned n = lua_rawlen(L, -1);
			for (unsigned i = 1; i <= n; i++) {
				lua_rawgeti(L, -1, i);
				const char* name = lua_tostring(L, -1);
				bool do_run = ImGui::Button(name);
				if (do_run) {
					lua_getglobal(L, "run_view");
					lua_pushvalue(L, -2);
					int e = ecall(L, 1, 0);
					if (e != 0) {
						handle_lua_error();
					}
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}

	if (g.lua_error) {
		ImGui::TextUnformatted(g.lua_error_message);
	}

	ImGui::Text("TOP: %d", lua_gettop(L));
}

void iced_gui(void)
{
	check_for_reload();

	static bool show_main = true;
	if (show_main) {
		if (ImGui::Begin("Main", &show_main)) {
			lua_stuff();
		}
		ImGui::End();
	}
}
