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

static void l_load(lua_State* L, const char* path)
{
	watch_file(path);
	if (g.lua_error) return;
	int e = luaL_dofile(L, path);
	if (e != 0) {
		g.lua_error = true;
		const char* err = lua_tostring(L, -1);
		fprintf(stderr, "LUA ERROR: %s\n", err);
		snprintf(g.lua_error_message, sizeof g.lua_error_message, "%s", err);
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

void iced_gui(void)
{
	check_for_reload();

	static bool show_main = true;
	if (show_main) {
		if (ImGui::Begin("Main", &show_main)) {
			ImGui::Button("TODO");
			if (g.lua_error) {
				ImGui::TextUnformatted(g.lua_error_message);
			}
		}
		ImGui::End();
	}
}
