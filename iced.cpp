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

struct node root_node;

static bool node_is_leaf(struct node* node)
{
	switch (node->type) {
	case NILNODE:
		return true;
	case PROCEDURE:
	case MATERIAL:
	case WORK_LIGHT:
	case NAVMESH_GEN:
	case ENTITY:
		return true;
	case VIEW3D:
	case VIEW2D:
	case RAYMARCH_VIEW:
	case PATHMARCH_RENDER:
	case OPTIMIZE:
		return false;
	}
	int t = (int)node->type;
	if (0 <= t && t < n_nodedefs) {
		switch (nodedefs[t].type) {
		case SDF3D:
		case SDF2D:
			return true;
		case TX3D:
		case TX2D:
		case VOLUMIZE:
		case D1:
		case D2:
			return false;
		}
	}
	assert(!"unreachable");
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


void iced_init(void)
{
	nodedef_init();

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
}

static void ui_node(struct node* node)
{
	//const ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_SpanFullWidth;
	//const ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_SpanAvailWidth;
	const ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_SpanAllColumns;
	const ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow;
	const ImGuiTreeNodeFlags leaf_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	char* name = node->name;
	char type_str[1<<14];
	char* tp = type_str;
	for (;;) {
		tp += sprintf(tp, "%s%d", tp>type_str?",":"", node->type);
		if (node->inline_child) {
			assert(arrlen(node->child_arr) == 1);
			node = &node->child_arr[0];
		} else {
			break;
		}
	}

	const int n_childs = arrlen(node->child_arr);
	const bool is_leaf = node_is_leaf(node);
	bool recurse = false;
	if (is_leaf && n_childs == 0) {
		ImGui::TreeNodeEx("Leaf", base_flags | leaf_flags, "%s", name);
	} else {
		recurse = ImGui::TreeNodeEx("Node", base_flags | node_flags, "%s", name);
	}

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
		printf("show node stuff\n");
	}

	if (recurse) {
		for (int i = 0; i < n_childs; i++) {
			ui_node(&node->child_arr[i]);
		}
		ImGui::TreePop();
	}

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(type_str);
}

static void ui_tree(void)
{
	const ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
	if (ImGui::BeginTable("Table", 2, table_flags)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		ui_node(&root_node);

		ImGui::EndTable();
	}
}

void iced_gui(void)
{
	if (show_window_tree) {
		if (ImGui::Begin("Tree", &show_window_tree)) {
			if (ImGui::Button("CODEGEN")) iced_codegen(&root_node); // XXX
			ui_tree();
		}
		ImGui::End();
	}

	if (show_window_node) {
		if (ImGui::Begin("Node", &show_window_node)) {
		}
		ImGui::End();
	}
}
