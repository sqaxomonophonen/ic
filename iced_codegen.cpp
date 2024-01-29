#include <assert.h>
#include <stdio.h>
#include "stb_ds.h"
#include "iced.h"

static int* nodedef_tag_arr;

static char* P0 = NULL;
static char* P = NULL;
static char* P1 = NULL;

static void addsrc(const char* n0, const char* n1, const char* src)
{
	if (src == NULL) return;
	P += snprintf(P, P1-P, "#define FN %s%s%s\n", n0, n1==NULL?"":"_", n1==NULL?"":n1);
	const size_t n = strlen(src);
	assert((P+n)<P1);
	memcpy(P, src, n);
	P += n;
	assert(P<P1);
	P += snprintf(P, P1-P, "#undef FN\n");
	assert(P<P1);
}

static void trace(struct node* node)
{
	if (0 < node->type && node->type < n_nodedefs && nodedef_tag_arr[node->type] == 0) {
		nodedef_tag_arr[node->type] = 1;
		struct nodedef* def = &nodedefs[node->type];
		const char* n = def->name;
		switch (def->type) {
		case SDF3D:
			addsrc(n, NULL, def->sdf3d.glsl_sdf3d);
			break;
		case SDF2D:
			addsrc(n, NULL, def->sdf2d.glsl_sdf2d);
			break;
		case TX3D:
			addsrc(n, "tx", def->tx3d.glsl_tx3d);
			addsrc(n, "d1", def->tx3d.glsl_d1);
			break;
		case TX2D:
			addsrc(n, "tx", def->tx2d.glsl_tx2d);
			addsrc(n, "d1", def->tx2d.glsl_d1);
			break;
		case D1:
			addsrc(n, NULL, def->d1.glsl_d1);
			break;
		case D2:
			addsrc(n, "d2", def->d2.glsl_d2);
			//addsrc(n, "d2m", def->d2.glsl_d2m);
			break;
		case VOLUMIZE:
			addsrc(n, "tx", def->volumize.glsl_tx);
			addsrc(n, "d1", def->volumize.glsl_d1);
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


void iced_codegen(struct node* root)
{
	if (P0 == NULL) {
		const size_t n = 1<<18;
		P0 = (char*)malloc(n);
		P1 = P0+n;
	}
	P = P0;

	const int n = n_nodedefs;
	arrsetlen(nodedef_tag_arr, n);
	memset(nodedef_tag_arr, 0, n*sizeof(nodedef_tag_arr[0]));

	trace(root);

	printf("CODEGEN ::\n%s\n", P0);
}
