#include <assert.h>
#include <stdio.h>
#include "stb_ds.h"
#include "iced.h"

static int* nodedef_tag_arr;

static char* P0 = NULL;
static char* P = NULL;
static char* P1 = NULL;

static void addraw(const char* src, size_t n)
{
	if (src == NULL) return;
	assert((P+n+1)<P1);
	memcpy(P, src, n);
	P[n] = 0;
	P += n;
}

static void addsrc(const char* src)
{
	addraw(src, strlen(src));
}

static void trace(struct node* node)
{
	if (0 < node->type && node->type < n_nodedefs) {
		if (!nodedef_tag_arr[node->type]) {
			nodedef_tag_arr[node->type] = 1;
			struct nodedef* def = &nodedefs[node->type];
			for (int i = 0; i < def->n_glsl_fns; i++) {
				struct glsl_fn* fn = &def->glsl_fns[i];
				addsrc(fn->src);
			}
		}
	}

	const int n = arrlen(node->child_arr);
	for (int i = 0; i < n; i++) {
		trace(&node->child_arr[i]);
	}
}

static void genmap_rec(struct node* node)
{
	if (0 < node->type && node->type < n_nodedefs) {
		struct nodedef* def = &nodedefs[node->type];
		switch (def->type) {
		case SDF3D:
			break;
		case SDF2D:
			break;
		case TX3D:
			break;
		case TX2D:
			break;
		case D1:
			break;
		case D2:
			break;
		case VOLUMIZE:
			break;
		default:
			assert(!"unhandled type");
		}
	}
}

static void genmap(struct node* node)
{
	addsrc(
	"float map(vec3 p)\n"
	"{\n"
	);
	genmap_rec(node);
	addsrc(
	"}\n"
	);
}

void iced_codegen(struct node* root)
{
	if (P0 == NULL) {
		const size_t n = 1<<18;
		P0 = (char*)malloc(n);
		P1 = P0+n;
	}
	P = P0;

	arrsetlen(nodedef_tag_arr, n_nodedefs);
	memset(nodedef_tag_arr, 0, n_nodedefs*sizeof(nodedef_tag_arr[0]));

	trace(root);

	genmap(root);

	printf("CODEGEN ::\n%s\n", P0);
}
