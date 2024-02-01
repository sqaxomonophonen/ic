#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "stb_ds.h"
#include "iced.h"

static int* nodedef_tag_arr;

// src output buffer
static char* P0 = NULL;
static char* P = NULL;
static char* P1 = NULL;

// tmp space
static char* T0 = NULL;
static char* T = NULL;
static char* T1 = NULL;

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

static int var_serial;
static const char* mkvar(const char* prefix)
{
	const char* s = T;
	T += snprintf(T, T1-T, "%s%d", prefix, var_serial++)+1;
	return s;
}

__attribute__((format (printf, 1, 2)))
static void srcf(const char* fmt, ...)
{
	char buf[1<<12];
	va_list args;
	va_start(args, fmt);
	addraw(buf, vsnprintf(buf, sizeof buf, fmt, args));
	va_end(args);
}

static void calltail(struct nodedef* def, struct node* nod)
{
	const int n = def->n_args;
	for (int i = 0; i < n; i++) {
		struct nodedef_arg* darg = &def->args[i];
		union nodearg* arg = &nod->arg_arr[i];
		switch (darg->type) {
		case RADIUS:
		case SCALAR:
			srcf(",%f", arg->f1);
			break;
		case DIM3D:
		case POS3D:
			srcf(",vec3(%f,%f,%f)", arg->v3.x, arg->v3.y, arg->v3.z);
			break;
		case POS2D:
			srcf(",vec3(%f,%f)", arg->v2.x, arg->v2.y);
			break;
		default: assert(!"unhandled type");
		}
	}
	srcf(");\n");
}

static const char* maprec(struct node* node, const char* pvar, int dim)
{
	bool do_implicit_union = false;

	switch (node->type) {
	case PROCEDURE:
	case WORK_LIGHT:
	case NAVMESH_GEN:
	case ENTITY:
		return NULL;
	case NILNODE:
	case VIEW3D:
	case VIEW2D:
	case RAYMARCH_VIEW:
	case PATHMARCH_RENDER:
		do_implicit_union = true;
		break;
	case MATERIAL:
		// TODO material?
		do_implicit_union = true;
		break;
	case OPTIMIZE:
		// FIXME I suppose first child is real subtree, and second
		// child is bounding volume. what if there's 3+ childs?
		assert(!"TODO optimize node");
		break;
	}

	const char* pv0 = pvar;
	int pass_dim = dim;
	struct nodedef* def = (0 < node->type && node->type < n_nodedefs) ? &nodedefs[node->type] : NULL;
	struct nodedef* reduce_def = NULL;
	const char* reduce_fn = NULL;
	if (def != NULL) {
		const char* pv1 = NULL;
		const char* fn = NULL;
		switch (def->type) {

		case TX3D:
		case TX2D:
			assert((def->type == TX3D && dim == 3) || (def->type == TX2D && dim == 2));
			fn = nodedef_get_glsl_fn_name(def, def->type == TX3D ? FN_TX3D_TX : def->type == TX2D ? FN_TX2D_TX : (enum glsl_fn_type)-1);
			assert(fn != NULL);
			pv1 = mkvar("p");
			srcf("\tvec%d %s = %s(%s", dim, pv1, fn, pv0);
			calltail(def, node);
			pv0 = pv1;
			do_implicit_union = true;
			break;

		case VOLUMIZE:
			assert(dim == 3);
			pass_dim = 2;
			fn = nodedef_get_glsl_fn_name(def, FN_VOLUMIZE_TX);
			assert(fn != NULL);
			pv1 = mkvar("p");
			srcf("\tvec%d %s = %s(%s", pass_dim, pv1, fn, pv0);
			calltail(def, node);
			pv0 = pv1;
			do_implicit_union = true;
			break;

		case D1:
			do_implicit_union = true;
			break;

		case D2:
			reduce_def = def;
			reduce_fn = nodedef_get_glsl_fn_name(def, FN_D2_D2);
			break;

		}
	}

	const char* dvar = NULL;

	struct nodedef* un = &nodedefs[union_nodedef_index];
	assert((un->n_args == 0) && "not expecting arguments for built-in union");
	const char* unfn = nodedef_get_glsl_fn_name(un, FN_D2_D2);
	assert(unfn != NULL);

	if (do_implicit_union || reduce_def != NULL) {
		const int n = arrlen(node->child_arr);
		for (int i = 0; i < n; i++) {
			const char* dv1 = maprec(&node->child_arr[i], pv0, pass_dim);
			if (dv1 == NULL) continue;
			if (dvar == NULL) {
				dvar = dv1;
			} else {
				const char* dv2 = mkvar("d");
				srcf("\tfloat %s = %s(%s, %s);\n",
					dv2,
					do_implicit_union ? unfn :
					reduce_def != NULL ? reduce_fn :
					"XXX",
					dvar,
					dv1);
				dvar = dv2;
			}
		}
	}

	if (def != NULL) {
		const char* dv1 = NULL;
		const char* fn = NULL;

		switch (def->type) {

		case SDF3D:
		case SDF2D:
			assert((def->type == SDF3D && dim == 3) || (def->type == SDF2D && dim == 2));
			assert(arrlen(node->child_arr) == 0);
			assert(dvar == NULL);
			dvar = mkvar("d");
			fn = nodedef_get_glsl_fn_name(def, def->type == SDF3D ? FN_SDF3D : def->type == SDF2D ? FN_SDF2D : (enum glsl_fn_type)-1);
			assert(fn != NULL);
			srcf("\tfloat %s = %s(%s", dvar, fn, pv0);
			calltail(def, node);
			break;

		case D1:
			fn = nodedef_get_glsl_fn_name(def, FN_D1);
			assert(fn != NULL);
			dv1 = mkvar("d");
			srcf("\tfloat %s = %s(%s", dv1, fn, dvar);
			calltail(def, node);
			break;

		case VOLUMIZE:
			assert(dim == 3);
			fn = nodedef_get_glsl_fn_name(def, FN_VOLUMIZE_D1);
			if (fn != NULL) {
				assert(dvar != NULL);
				dv1 = mkvar("d");
				srcf("\tfloat %s = %s(%s", dv1, fn, dvar);
				calltail(def, node);
				dvar = dv1;
			}
			break;

		}
	}

	return dvar;
}

static void genmap(struct node* node)
{
	const int dim = 3; // XXX where should this come from?
	const char* p = mkvar("p");
	srcf(
		"float map(vec%d %s)\n"
		"{\n",
		dim, p);
	const char* r = maprec(node, p, dim);
	srcf(
		"	return %s;\n"
		"}\n",
		r);
}

void iced_codegen(struct node* root)
{
	if (P0 == NULL) {
		const size_t n = 1<<18;
		P0 = (char*)malloc(n);
		P1 = P0+n;
	}
	P = P0;

	if (T0 == NULL) {
		const size_t n = 1<<18;
		T0 = (char*)malloc(n);
		T1 = T0+n;
	}
	T = T0;

	var_serial = 0;

	arrsetlen(nodedef_tag_arr, n_nodedefs);
	memset(nodedef_tag_arr, 0, n_nodedefs*sizeof(nodedef_tag_arr[0]));

	trace(root);

	genmap(root);

	printf("CODEGEN ::\n%s\n", P0);
}
