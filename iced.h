#ifndef ICED_H

#include "gb_math.h"

#define MAX_NODEDEF_ARGS (4)
#define MAX_NODEDEF_GLSL_FNS (2)
#define MAX_NODEDEFS (1<<12)

union nodearg {
	float f1;
	gbVec2 v2;
	gbVec3 v3;
	gbVec4 v4;
};

typedef void (*affine_transform_fn)(gbMat4* m, union nodearg* args);

enum nodedef_arg_type {
	RADIUS = 1,
	SCALAR,
	DIM3D,
	POS3D,
	POS2D,
};

struct nodedef_arg {
	const char* name;
	enum nodedef_arg_type type;
};

#define EMIT_NODEDEF_TYPES \
	X(SDF3D) \
	X(SDF2D) \
	X(TX3D) \
	X(TX2D) \
	X(VOLUMIZE) \
	X(D1) \
	X(D2)

enum nodedef_type {
	NILNODEDEF = 0,
	#define X(NAME) NAME,
	EMIT_NODEDEF_TYPES
	#undef X
};

static inline const char* get_nodedef_type_str(enum nodedef_type t)
{
	switch (t) {
	#define X(NAME) case NAME: return #NAME;
	EMIT_NODEDEF_TYPES
	#undef X
	}
	assert(!"unhandled type");
}

enum node_type {
	NILNODE = 0,

	// gap reserved for `nodedefs` indices

	SPECIAL_NODE_TYPES = 100000,
	//SCENE, //
	PROCEDURE, // subtree from lua code
	VIEW3D, // editor 3d view of subtree
	VIEW2D, // editor 2d view of subtree
	MATERIAL, // set material of subtree
	RAYMARCH_VIEW, // raymarching preview
	WORK_LIGHT, // work light for raymarch preview
	PATHMARCH_RENDER, // pathmarch triangulator final render
	OPTIMIZE, // bounding volume optimization
	NAVMESH_GEN, // navigation mesh generator
	ENTITY, // entity
};

enum glsl_fn_type {
	FN_SDF3D,
	FN_SDF2D,
	FN_TX3D_TX,
	FN_TX3D_D1,
	FN_TX2D_TX,
	FN_TX2D_D1,
	FN_D1,
	FN_D2_D2,
	FN_D2_D2M,
	FN_VOLUMIZE_TX,
	FN_VOLUMIZE_D1,
};

struct glsl_fn {
	enum glsl_fn_type type;
	const char* name;
	const char* src;
};

struct nodedef {
	enum nodedef_type type;
	const char* name;
	int n_args;
	struct nodedef_arg args[MAX_NODEDEF_ARGS];
	int n_glsl_fns;
	struct glsl_fn glsl_fns[MAX_NODEDEF_GLSL_FNS];
	affine_transform_fn fn_affine;
};

struct node {
	enum node_type type;
	union nodearg* arg_arr;
	char* name;
	// TODO flag mask?
	bool inline_child; // show child node as inline (only valid if arrlen(child_arr)==1)
	//bool invalid_context; // XXX derived? do I have to cache it?
	struct node* child_arr;
};

void iced_init(void);
void iced_gui(void);

void nodedef_init(void);
int nodedef_find(enum nodedef_type type, const char* name);
const char* nodedef_get_glsl_fn_name(struct nodedef*, enum glsl_fn_type);

void parse_file(const char* path, struct node* root);
void iced_codegen(struct node* root);

extern int n_nodedefs;
extern struct nodedef nodedefs[MAX_NODEDEFS];
extern int union_nodedef_index;

#define ICED_H
#endif
