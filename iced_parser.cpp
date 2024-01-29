#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stb_ds.h"
#include "util.h"
#include "iced.h"

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
	struct node* root;
};

static void parser_init(struct parser* ps, struct node* root)
{
	memset(ps, 0, sizeof *ps);
	ps->st = PST0;
	ps->root = root;
}

static struct node* parser_top(struct parser* ps)
{
	const int n = arrlen(ps->nodestack_arr);
	return n == 0 ? NULL : &ps->nodestack_arr[n-1];
}

static void parser_error(struct parser* ps, const char* msg)
{
	fprintf(stderr, "PARSE ERROR: %s\n", msg);
	ps->st = PST_ERROR;
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
	case PST_ERROR:   return PARSER_ERROR;

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
			if (arrlen(ps->nodestack_arr) == 0) {
				parser_error(ps, "endnode without node");
			} else {
				struct node nn = arrpop(ps->nodestack_arr);
				struct node* dn;
				if (arrlen(ps->nodestack_arr) == 0) {
					dn = ps->root;
				} else {
					dn = parser_top(ps);
				}
				arrput(dn->child_arr, nn);
			}
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
			parser_error(ps, "unhandled string");
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
			parser_error(ps, "unhandled node type");
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
				parser_error(ps, "unhandled node type\n");
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
	default:
		parser_error(ps, "unexpected EOL");
		break;
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

void parse_file(const char* path, struct node* root)
{
	size_t sz;
	const char* source = read_file(path, &sz);
	if (source == NULL) return;
	struct parser ps;
	parser_init(&ps, root);

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
				parser_error(&ps, "expected EOL");
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
				parser_error(&ps, "expected token");
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
					parser_error(&ps, "expected float");
					parse_error = true;
					break;
				}
				parser_push_float(&ps, f);
				} break;
			case EXPECT_INT: {
				long i = strtol(tmp, &endptr, 10);
				if (endptr == tmp) {
					parser_error(&ps, "expected int");
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
