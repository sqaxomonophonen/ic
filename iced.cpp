#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

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


void iced_init(void)
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	assert(luaL_dofile(L, "lib.lua") == 0);
	assert(luaL_dofile(L, "world.lua") == 0);
}

void iced_gui(void)
{
	static bool show_main = true;
	if (show_main) {
		if (ImGui::Begin("Main", &show_main)) {
			ImGui::Button("TODO");
		}
		ImGui::End();
	}
}
