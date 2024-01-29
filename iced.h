#ifndef ICED_H

#include "gb_math.h"

#define MAX_NODEDEF_ARGS (4)
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

enum nodedef_type {
	NILNODE = 0,

	SDF3D = 100000,
	SDF2D,
	TX3D,
	TX2D,
	VOLUMIZE,
	D1,
	D2,

	SPECIAL_NODEDEFS = 110000,
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

struct nodedef_sdf3d {
	const char* glsl_sdf3d;
};

struct nodedef_sdf2d {
	const char* glsl_sdf2d;
};

struct nodedef_tx3d {
	const char* glsl_tx3d;
	const char* glsl_d1;
	affine_transform_fn fn_affine;
};

struct nodedef_tx2d {
	const char* glsl_tx2d;
	const char* glsl_d1;
	affine_transform_fn fn_affine;
};

struct nodedef_d1 {
	const char* glsl_d1;
};

struct nodedef_d2 {
	const char* glsl_d2;
	const char* glsl_d2m;
};

struct nodedef_volumize {
	const char* glsl_tx;
	const char* glsl_d1;
};

struct nodedef {
	enum nodedef_type type;
	const char* name;
	int n_args;
	struct nodedef_arg args[MAX_NODEDEF_ARGS];
	union {
		struct nodedef_sdf3d sdf3d;
		struct nodedef_sdf2d sdf2d;
		struct nodedef_tx3d tx3d;
		struct nodedef_tx2d tx2d;
		struct nodedef_d1 d1;
		struct nodedef_d2 d2;
		struct nodedef_volumize volumize;
	};
};

struct node {
	int type; // nodedef index, or SPECIAL_NODEDEFS+
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

extern int n_nodedefs;
extern struct nodedef nodedefs[MAX_NODEDEFS];

#define ICED_H
#endif
