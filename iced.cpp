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

enum parser_expect_type {
	PARSER_ERROR,
	EXPECT_SYMBOL,
	EXPECT_SYMBOL_OR_EOL,
	EXPECT_STRING_UNTIL_EOL,
	EXPECT_FLOAT,
	EXPECT_INT,
	EXPECT_EOL,
};

enum parser_state {
	PST0 = 0,
	PST_NODE,
	PST_SCENE,
	PST_MATERIAL,
	PST_ARG0,
	PST_ARG1,
	PST_NAME,
	PST_FLAGS,
	PST_EOSTMT,
	PST_ERROR,
};

struct parser {
	enum parser_state st;
	int depth;
	struct node* nodestack_arr;
	union nodearg* nodearg_dst;
	enum nodedef_arg_type nodearg_dst_type;
	int seq;
};

static void parser_init(struct parser* ps)
{
	memset(ps, 0, sizeof *ps);
	ps->st = PST0;
}

static struct node* parser_top(struct parser* ps)
{
	const int n = arrlen(ps->nodestack_arr);
	return n == 0 ? NULL : &ps->nodestack_arr[n-1];
}

static enum parser_expect_type handle_arg1(struct parser* ps, float** dst, int* out_n)
{
	assert(ps->st == PST_ARG1);
	int n = -1;
	float* fp = NULL;
	switch (ps->nodearg_dst_type) {
	case RADIUS:
	case SCALAR:
		n = 1;
		fp = &ps->nodearg_dst->f1;
		break;
	case DIM3D:
	case POS3D:
		n = 3;
		fp = &ps->nodearg_dst->v3.e[ps->seq];
		break;
	case POS2D:
		n = 2;
		fp = &ps->nodearg_dst->v2.e[ps->seq];
		break;
	default: assert(!"unhandled type");
	}
	assert(n >= 1);
	if (dst != NULL) *dst = fp;
	if (out_n != NULL) *out_n = n;
	return ps->seq >= n ? EXPECT_EOL : EXPECT_FLOAT;
}

static enum parser_expect_type parser_next(struct parser* ps)
{
	switch (ps->st) {
	case PST0:        return EXPECT_SYMBOL_OR_EOL;
	case PST_EOSTMT:  return EXPECT_EOL;

	case PST_NODE:
	case PST_SCENE:
	case PST_MATERIAL:
	case PST_ARG0:
		return EXPECT_SYMBOL;

	case PST_NAME:  return EXPECT_STRING_UNTIL_EOL;
	case PST_FLAGS: return EXPECT_SYMBOL_OR_EOL;

	case PST_ARG1: return handle_arg1(ps, NULL, NULL);

	default: assert(!"unhandled state");
	}
}

static void parser_push_float(struct parser* ps, double f)
{
	if (ps->st == PST_ARG1) {
		float* dst = NULL;
		int n = -1;
		handle_arg1(ps, &dst, &n);
		assert(dst != NULL);
		*dst = f;
		ps->seq++;
		if (ps->seq == n) ps->st = PST_EOSTMT;
	} else {
		assert(!"unhandled state");
	}
}

static void parser_push_int(struct parser* ps, long i)
{
	assert(!"TODO");
}

static bool strmemeq(const char* c_str, const char* s0, const char* s1)
{
	const size_t n = strlen(c_str);
	if (n != (s1-s0)) return false;
	return memcmp(c_str, s0, n) == 0;
}

static void parser_push_string(struct parser* ps, const char* s0, const char* s1)
{
	switch (ps->st) {
	case PST0:
		if (strmemeq("node", s0, s1)) {
			ps->st = PST_NODE;
		} else if (strmemeq("scene", s0, s1)) {
			ps->st = PST_SCENE;
		} else if (strmemeq("material", s0, s1)) {
			ps->st = PST_MATERIAL;
		} else if (strmemeq("endnode", s0, s1)) {
			// TODO commit node?
			ps->st = PST_EOSTMT;
		} else if (strmemeq("endscene", s0, s1)) {
			// TODO commit scene?
			ps->st = PST_EOSTMT;
		} else if (strmemeq("endmaterial", s0, s1)) {
			// TODO commit material?
			ps->st = PST_EOSTMT;
		} else if (strmemeq("arg", s0, s1)) {
			ps->st = PST_ARG0;
		} else if (strmemeq("name", s0, s1)) {
			ps->st = PST_NAME;
		} else if (strmemeq("flags", s0, s1)) {
			ps->st = PST_FLAGS;
		} else {
			fprintf(stderr, "PARSER ERROR: unhandled string\n");
			ps->st = PST_ERROR;
		}
		break;
	case PST_NODE: {
		int type = -1;

		if (type == -1) {
			for (int i = 0; i < n_nodedefs; i++) {
				if (strmemeq(nodedefs[i].name, s0, s1)) {
					type = i;
					break;
				}
			}
		}

		if (type == -1) {
			if (strmemeq("procedure", s0, s1)) {
				type = PROCEDURE;
			} else if (strmemeq("view3d", s0, s1)) {
				type = VIEW3D;
			} else if (strmemeq("view2d", s0, s1)) {
				type = VIEW2D;
			} else if (strmemeq("material", s0, s1)) {
				type = MATERIAL;
			} else if (strmemeq("raymarch_view", s0, s1)) {
				type = RAYMARCH_VIEW;
			} else if (strmemeq("work_light", s0, s1)) {
				type = WORK_LIGHT;
			}
			// TODO more?
		}

		if (type == -1) {
			fprintf(stderr, "PARSER ERROR: unhandled node type\n");
			ps->st = PST_ERROR;
			break;
		}

		struct node* node = arraddnptr(ps->nodestack_arr, 1);
		assert(node == parser_top(ps));
		memset(node, 0, sizeof *node);
		node->type = type;

		ps->st = PST_EOSTMT;

		} break;
	case PST_NAME: {
		struct node* node = parser_top(ps);
		const size_t n = s1-s0;
		node->name = (char*)malloc(n+1);
		memcpy(node->name, s0, n);
		node->name[n] = 0;
		ps->st = PST0;
		} break;
	case PST_ARG0: {
		struct node* node = parser_top(ps);
		const int t = node->type;
		if (0 < t && t < n_nodedefs) {
			struct nodedef* def = &nodedefs[t];
			arrsetlen(node->arg_arr, def->n_args);
			int index = -1;
			enum nodedef_arg_type at;
			for (int i = 0; i < def->n_args; i++ ) {
				struct nodedef_arg* a = &def->args[i];
				if (strmemeq(a->name, s0, s1)) {
					index = i;
					at = a->type;
					break;
				}
			}
			if (index == -1) {
				fprintf(stderr, "PARSER ERROR: unhandled node type\n");
				ps->st = PST_ERROR;
				break;
			}
			assert(index >= 0);
			ps->nodearg_dst = &node->arg_arr[index];
			ps->nodearg_dst_type = at;
			ps->seq = 0;
			ps->st = PST_ARG1;
		} else {
			assert(!"TODO special args?");
		}
		} break;
	default: assert(!"unhandled state");
	}
}

static void parser_push_eol(struct parser* ps)
{
	switch (ps->st) {
	case PST0:
	case PST_EOSTMT:
		ps->st = PST0;
		return;
	case PST_ARG1:
		// TODO check correct state? this allows early arg termination
		ps->st = PST0;
		return;
	default: assert(!"unhandled state");
	}
}

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

static void parse_file(const char* path)
{
	size_t sz;
	const char* source = read_file(path, &sz);
	if (source == NULL) return;
	struct parser ps;
	parser_init(&ps);

	bool parse_error = false;
	const char* p = source;
	while (!parse_error) {
		enum parser_expect_type expect_type = parser_next(&ps);
		if (expect_type == PARSER_ERROR) {
			parse_error = true;
			fprintf(stderr, "PARSER ERROR\n");
			break;
		}

		bool is_eol = false;

		for (;;) {
			if (*p == ' ' || *p == '\t') {
				p++;
			} else {
				break;
			}
		}

		while (*p == '\n' || *p == '\r') {
			p++;
			is_eol = true;
		}
		if (*p == 0) {
			parser_push_eol(&ps);
			break;
		}

		bool eat_token = false;
		switch (expect_type) {
		case EXPECT_EOL:
			if (is_eol) {
				parser_push_eol(&ps);
			} else {
				fprintf(stderr, "PARSER ERROR: expected EOL\n");
				parse_error = true;
			}
			break;
		case EXPECT_STRING_UNTIL_EOL: {
			const char* p0 = p;
			while (*p != 0 && *p != '\n'&& *p != '\r') {
				p++;
			}
			parser_push_string(&ps, p0, p);
			parser_push_eol(&ps);
			} break;
		case EXPECT_SYMBOL_OR_EOL:
			if (is_eol) {
				parser_push_eol(&ps);
			} else {
				eat_token = true;
			}
			break;
		case EXPECT_SYMBOL:
		case EXPECT_FLOAT:
		case EXPECT_INT:
			eat_token = true;
			break;
		default: assert(!"unhandled type");
		}

		if (eat_token) {
			const char* p0 = p;
			while (*p != 0 && *p != '\n'&& *p != '\r' && *p != ' ' && *p != '\t') {
				p++;
			}

			if (p == p0) {
				fprintf(stderr, "PARSER ERROR: expected token\n");
				parse_error = true;
				break;
			}

			char tmp[1<<10];
			const size_t n = p-p0;
			assert(n < ARRAY_LENGTH(tmp));
			memcpy(tmp, p0, n);
			tmp[n] = 0;
			char* endptr = NULL;

			switch (expect_type) {
			case EXPECT_SYMBOL_OR_EOL:
			case EXPECT_SYMBOL:
				parser_push_string(&ps, p0, p);
				break;
			case EXPECT_FLOAT: {
				double f = strtod(tmp, &endptr);
				if (endptr == tmp) {
					fprintf(stderr, "PARSER ERROR: expected float\n");
					parse_error = true;
					break;
				}
				parser_push_float(&ps, f);
				} break;
			case EXPECT_INT: {
				long i = strtol(tmp, &endptr, 10);
				if (endptr == tmp) {
					fprintf(stderr, "PARSER ERROR: expected int\n");
					parse_error = true;
					break;
				}
				parser_push_int(&ps, i);
				} break;
			default: assert(!"unhandled type");
			}
			if (*p == 0 || *p == '\n' || *p == '\r') {
				parser_push_eol(&ps);
			}
		}
	}

	free((void*)source);
}

void iced_init(void)
{
	nodedef_init();
	parse_file("ic.tree");
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
}
