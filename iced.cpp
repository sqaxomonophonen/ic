#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "imgui.h"
#include "util.h"
#include "iced.h"

static bool show_window_scene_tree = true;

void iced_init(void)
{
	nodedef_init();
}

void iced_gui(void)
{
	if (show_window_scene_tree) {
		if (ImGui::Begin("Scene Tree", &show_window_scene_tree)) {
			ImGui::Button("test");
			//const ImGuiTableFlags table_flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
			const ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
			//const ImGuiTreeNodeFlags base_tree_flags = ImGuiTreeNodeFlags_SpanAllColumns;
			const ImGuiTreeNodeFlags base_tree_flags = ImGuiTreeNodeFlags_SpanFullWidth;
			const ImGuiTreeNodeFlags leaf_tree_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			if (ImGui::BeginTable("Table", 4, table_flags)) {
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("C0", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("C1", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("C2", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableHeadersRow();

				{
					ImGui::PushID(0);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					bool open = ImGui::TreeNodeEx("Node 1?", base_tree_flags);
					if (open) {
						// TODO recursive render
						ImGui::TreePop();
					}

					ImGui::TableNextColumn();
					static bool st0 = true;
					ImGui::Checkbox("##c0", &st0);

					ImGui::TableNextColumn();
					static bool st1 = true;
					ImGui::Checkbox("##c1", &st1);

					ImGui::TableNextColumn();
					static bool st2 = true;
					ImGui::Checkbox("##c2", &st2);
					ImGui::PopID();
				}

				{
					ImGui::PushID(1);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TreeNodeEx("Node 2?", base_tree_flags | leaf_tree_flags);

					ImGui::TableNextColumn();
					static bool st0 = true;
					ImGui::Checkbox("##c0", &st0);

					ImGui::TableNextColumn();
					static bool st1 = true;
					ImGui::Checkbox("##c1", &st1);

					ImGui::TableNextColumn();
					static bool st2 = true;
					ImGui::Checkbox("##c2", &st2);
					ImGui::PopID();
				}

				ImGui::EndTable();
			}
		}
		ImGui::End();
	}
}
