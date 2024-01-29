#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "imgui.h"
#include "util.h"
#include "iced.h"

#include "stb_ds.h"

static bool show_window_tree = true;
static bool show_window_node = true;

struct node root_node;

static enum nodedef_type get_nodedef_type_from_node_type(int type)
{
	if ((enum nodedef_type)type > SPECIAL_NODEDEFS) return (enum nodedef_type)type;
	assert(0 <= type && type < n_nodedefs);
	return nodedefs[type].type;
}

static bool nodedef_type_is_leaf(enum nodedef_type t)
{
	switch (t) {
	case NILNODE:
	case SDF3D:
	case SDF2D:
	case WORK_LIGHT:
	case ENTITY:
		return true;

	case TX3D:
	case TX2D:
	case VOLUMIZE:
	case D1:
	case D2:
	case VIEW3D:
	case VIEW2D:
	case RAYMARCH_VIEW:
	case PATHMARCH_RENDER:
	case MATERIAL:
	case OPTIMIZE:
	case PROCEDURE:
	//case SCENE: // I think?
	case NAVMESH_GEN: // I think?
		return false;

	default:
		assert(!"unhandled type");
	}
}

void iced_init(void)
{
	nodedef_init();
	parse_file("ic.tree", &root_node);
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
	const bool is_leaf = nodedef_type_is_leaf(get_nodedef_type_from_node_type(node->type));
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
			ImGui::Button("Insert subtree");
			ImGui::SameLine();
			ImGui::Button("+node");

			ui_tree();
		}
		ImGui::End();
	}

	if (show_window_node) {
		if (ImGui::Begin("Node", &show_window_node)) {
		}
		ImGui::End();
	}

	iced_codegen(&root_node); // XXX
}
