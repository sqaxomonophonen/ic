#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
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
	EXPECT_BOOL,
	EXPECT_EOL,
};

enum parser_state {
	PST0 = 0,
	PST_NODE,
	PST_ARG0,
	PST_ARG1,
	PST_SET0,
	PST_SET1,
	PST_EOSTMT,
	PST_ERROR,
};

struct parser {
	int line;
	enum parser_state st;
	int depth;
	struct node* nodestack_arr;
	union nodearg* nodearg_dst;
	enum nodedef_arg_type nodearg_dst_type;
	int seq;
	struct node* root;
	enum parser_expect_type set1_type;
	void* set1_ptr;
};

static void parser_init(struct parser* ps, struct node* root)
{
	memset(ps, 0, sizeof *ps);
	ps->st = PST0;
	ps->root = root;
	ps->line = 1;
}

static struct node* parser_top(struct parser* ps)
{
	const int n = arrlen(ps->nodestack_arr);
	return n == 0 ? NULL : &ps->nodestack_arr[n-1];
}

__attribute__((format (printf, 2, 3)))
static void parser_error(struct parser* ps, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "PARSER ERROR at line %d: ", ps->line);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
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
	case PST_ARG0:
	case PST_SET0:
		return EXPECT_SYMBOL;

	case PST_ARG1: return handle_arg1(ps, NULL, NULL);
	case PST_SET1: return ps->set1_type;

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
	} else if (ps->st == PST_SET1) {
		*(float*)ps->set1_ptr = f;
		ps->st = PST_EOSTMT;
	} else {
		assert(!"unhandled state");
	}
}

static void parser_push_bool(struct parser* ps, bool b)
{
	if (ps->st == PST_SET1) {
		*(bool*)ps->set1_ptr = b;
		ps->st = PST_EOSTMT;
	} else {
		assert(!"unhandled state");
	}
}


static void parser_push_int(struct parser* ps, long i)
{
	if (ps->st == PST_SET1) {
		*(int*)ps->set1_ptr = i;
	} else {
		assert(!"unhandled state");
	}
}

static bool strmemeq(const char* c_str, const char* s0, const char* s1)
{
	const size_t n = strlen(c_str);
	if (n != (s1-s0)) return false;
	return memcmp(c_str, s0, n) == 0;
}

static void mkcstr(char* dst, size_t dstcap, const char* s0, const char* s1)
{
	const size_t n = s1-s0;
	assert((dstcap >= n+1) && "string too big");
	memcpy(dst, s0, n);
	dst[n] = 0;
}

#define STACK_CSTR(NAME,S0,S1) \
	char NAME[1<<12]; \
	mkcstr(NAME, sizeof NAME, S0, S1);

static void setset(struct parser* ps, enum parser_expect_type type, void* ptr)
{
	ps->st = PST_SET1;
	ps->set1_type = type;
	ps->set1_ptr = ptr;
}

static void parser_push_string(struct parser* ps, const char* s0, const char* s1)
{
	switch (ps->st) {
	case PST0:
		if (strmemeq("node", s0, s1)) {
			ps->st = PST_NODE;
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
		} else if (strmemeq("arg", s0, s1)) {
			ps->st = PST_ARG0;
		} else if (strmemeq("set", s0, s1)) {
			ps->st = PST_SET0;
		} else {
			STACK_CSTR(str, s0, s1);
			parser_error(ps, "unhandled string [%s]", str);
		}
		break;
	case PST_NODE: {
		enum node_type type = (enum node_type)-1;

		if (type == -1) {
			const char* sc = NULL;
			for (const char* s = s0; s < s1; s++) {
				if (*s == ':') {
					sc = s;
					break;
				}
			}
			if (sc != NULL && s0 < sc && sc < (s1-1)) {
				enum nodedef_type dtype = (enum nodedef_type)-1;
				#define X(NAME) \
				if (dtype == -1 && strmemeq(#NAME, s0, sc)) { \
					dtype = NAME; \
				}
				EMIT_NODEDEF_TYPES
				#undef X
				if (dtype == -1) {
					STACK_CSTR(str, s0, s1);
					parser_error(ps, "unhandled node type [%s] (1)", str);
				} else {
					STACK_CSTR(name, sc+1, s1);
					int idx = nodedef_find(dtype, name);
					if (idx == -1) {
						STACK_CSTR(str, s0, s1);
						parser_error(ps, "unhandled node type [%s] (2)", str);
					} else {
						type = (enum node_type)idx;
					}
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
			STACK_CSTR(str, s0, s1);
			parser_error(ps, "unhandled node type [%s] (3)", str);
			break;
		}

		assert(type > 0);

		struct node* node = arraddnptr(ps->nodestack_arr, 1);
		assert(node == parser_top(ps));
		memset(node, 0, sizeof *node);
		node->type = type;

		ps->st = PST_EOSTMT;

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
				STACK_CSTR(str, s0, s1);
				parser_error(ps, "unhandled arg type [%s] (3)", str);
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
	case PST_SET0: {
		struct node* node = parser_top(ps);
		if (strmemeq("name", s0, s1)) {
			setset(ps, EXPECT_STRING_UNTIL_EOL, &node->name);
		} else if (strmemeq("inline", s0, s1)) {
			setset(ps, EXPECT_BOOL, &node->inline_child);
		} else {
			STACK_CSTR(str, s0, s1);
			parser_error(ps, "unhandled set-key [%s] (3)", str);
		}
		} break;
	case PST_SET1: {
		struct node* node = parser_top(ps);
		const size_t n = s1-s0;
		char* s = (char*)malloc(n+1);
		memcpy(s, s0, n);
		s[n] = 0;
		*(char**)ps->set1_ptr = s;
		ps->st = PST0;
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
		parser_error(ps, "unexpected EOL (st=%d)", ps->st);
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

		while (*p == '\n') {
			p++;
			ps.line++;
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
		case EXPECT_BOOL:
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
			case EXPECT_BOOL:
			case EXPECT_INT: {
				long i = strtol(tmp, &endptr, 10);
				if (endptr == tmp) {
					parser_error(&ps, "expected int");
					parse_error = true;
					break;
				}
				if (expect_type == EXPECT_BOOL) {
					parser_push_bool(&ps, i != 0);
				} else if (expect_type == EXPECT_INT) {
					parser_push_int(&ps, i);
				} else {
					assert(!"unreachable");
				}
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
