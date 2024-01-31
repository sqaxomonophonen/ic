#include <assert.h>
#include <stdio.h>
#include "stb_ds.h"
#include "iced.h"

static int* nodedef_tag_arr;

static char* P0 = NULL;
static char* P = NULL;
static char* P1 = NULL;

static void addsrc(const char* src)
{
	if (src == NULL) return;
	const size_t n = strlen(src);
	assert((P+n)<P1);
	memcpy(P, src, n);
	P += n;
}

static void addfn(const char* name, const char* postfix, const char* src)
{
	if (src == NULL) return;
	P += snprintf(P, P1-P, "#define FN %s%s%s\n", name, postfix==NULL?"":"_", postfix==NULL?"":postfix);
	addsrc(src);
	assert(P<P1);
	P += snprintf(P, P1-P, "#undef FN\n");
	assert(P<P1);
}

static const char* get_nodedef_type_str(enum nodedef_type t)
{
	switch (t) {
	#define X(NAME) case NAME: return #NAME;
	EMIT_NODEDEF_TYPES
	#undef X
	}
	assert(!"unhandled type");
}

static void trace(struct node* node)
{
	if (nodedef_tag_arr[node->type]) return;
	nodedef_tag_arr[node->type] = 1;

	if (0 < node->type && node->type < n_nodedefs) {
		struct nodedef* def = &nodedefs[node->type];
		char name[1<<12];
		snprintf(name, sizeof name, "%s_%s", get_nodedef_type_str(def->type), def->name);

		switch (def->type) {
		case SDF3D:
			addfn(name, NULL, def->sdf3d.glsl_sdf3d);
			break;
		case SDF2D:
			addfn(name, NULL, def->sdf2d.glsl_sdf2d);
			break;
		case TX3D:
			addfn(name, "tx", def->tx3d.glsl_tx3d);
			addfn(name, "d1", def->tx3d.glsl_d1);
			break;
		case TX2D:
			addfn(name, "tx", def->tx2d.glsl_tx2d);
			addfn(name, "d1", def->tx2d.glsl_d1);
			break;
		case D1:
			addfn(name, NULL, def->d1.glsl_d1);
			break;
		case D2:
			addfn(name, "d2", def->d2.glsl_d2);
			//addfn(name, "d2m", def->d2.glsl_d2m);
			break;
		case VOLUMIZE:
			addfn(name, "tx", def->volumize.glsl_tx);
			addfn(name, "d1", def->volumize.glsl_d1);
			break;
		default:
			assert(!"unhandled type");
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
