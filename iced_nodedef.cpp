#include <assert.h>
#include <stdlib.h>

#include "stb_ds.h"

#include "iced.h"

int n_nodedefs;
struct nodedef nodedefs[MAX_NODEDEFS];
static struct nodedef* def = NULL;

static void nodedef(enum nodedef_type type, const char* name)
{
	assert(def == NULL);
	assert((type != NILNODEDEF || n_nodedefs == 0) && "NILNODE must be first");
	assert(n_nodedefs < MAX_NODEDEFS);
	def = &nodedefs[n_nodedefs++];
	memset(def, 0, sizeof *def);
	def->type = type;
	def->name = name;
}

static void nodedef_end(void)
{
	assert(def != NULL);
	assert(def->name != NULL);
	switch (def->type) {
	case NILNODE:
		break;
	case SDF3D:
		assert(def->sdf3d.glsl_sdf3d != NULL);
		break;
	case SDF2D:
		assert(def->sdf2d.glsl_sdf2d != NULL);
		break;
	case TX3D:
		assert(def->tx3d.glsl_tx3d != NULL);
		break;
	case TX2D:
		assert(def->tx2d.glsl_tx2d != NULL);
		break;
	case VOLUMIZE:
		assert(def->volumize.glsl_tx != NULL);
		break;
	case D1:
		assert(def->d1.glsl_d1 != NULL);
		break;
	case D2:
		assert(def->d2.glsl_d2 != NULL);
		break;
	default: assert(!"unhandled type");
	}
	def = NULL;
}

static void glsl_sdf3d(const char* src)
{
	assert(def != NULL);
	assert(def->type == SDF3D);
	assert(def->sdf3d.glsl_sdf3d == NULL);
	def->sdf3d.glsl_sdf3d = src;
}

static void glsl_sdf2d(const char* src)
{
	assert(def != NULL);
	assert(def->type == SDF2D);
	assert(def->sdf2d.glsl_sdf2d == NULL);
	def->sdf2d.glsl_sdf2d = src;
}

static void glsl_tx3d(const char* src)
{
	assert(def != NULL);
	assert(def->type == TX3D);
	assert(def->tx3d.glsl_tx3d == NULL);
	def->tx3d.glsl_tx3d = src;
}

static void glsl_tx2d(const char* src)
{
	assert(def != NULL);
	assert(def->type == TX2D);
	assert(def->tx2d.glsl_tx2d == NULL);
	def->tx2d.glsl_tx2d = src;
}

static void glsl_tx(const char* src)
{
	assert(def != NULL);
	assert(def->type == VOLUMIZE);
	assert(def->volumize.glsl_tx== NULL);
	def->volumize.glsl_tx = src;
}

static void glsl_d1(const char* src)
{
	assert(def != NULL);
	if (def->type == TX2D) {
		assert(def->tx2d.glsl_d1 == NULL);
		def->tx2d.glsl_d1 = src;
	} else if (def->type == TX3D) {
		assert(def->tx3d.glsl_d1 == NULL);
		def->tx3d.glsl_d1 = src;
	} else if (def->type == VOLUMIZE) {
		assert(def->volumize.glsl_d1 == NULL);
		def->volumize.glsl_d1  = src;
	} else if (def->type == D1) {
		assert(def->d1.glsl_d1 == NULL);
		def->d1.glsl_d1  = src;
	} else {
		assert(!"bad type");
	}
}

static void glsl_d2(const char* src)
{
	assert(def != NULL);
	assert(def->type == D2);
	assert(def->d2.glsl_d2 == NULL);
	def->d2.glsl_d2 = src;
}

static void glsl_d2m(const char* src)
{
	assert(def != NULL);
	assert(def->type == D2);
	assert(def->d2.glsl_d2m == NULL);
	def->d2.glsl_d2m = src;
}

static void arg(const char* name, enum nodedef_arg_type type, int argidx)
{
	assert(def != NULL);
	assert(0 <= argidx && argidx < MAX_NODEDEF_ARGS);
	if (argidx >= def->n_args) def->n_args = argidx + 1;
	struct nodedef_arg* a = &def->args[argidx];
	a->name = name;
	a->type = type;
}

static void affine_translate_3d(gbMat4* m, union nodearg* args)
{
	gb_mat4_translate(m, args[0].v3);
}

static void affine_translate_2d(gbMat4* m, union nodearg* args)
{
	gb_mat4_translate(m, gb_vec3(args[0].v2.x, args[0].v2.y, 0));
}

static void affine_scale_3d(gbMat4* m, union nodearg* args)
{
	gb_mat4_scale(m, gb_vec3(args[0].f1, args[0].f1, args[0].f1));
}

static void affine_scale_2d(gbMat4* m, union nodearg* args)
{
	gb_mat4_scale(m, gb_vec3(args[0].f1, args[0].f1, 1));
}

static void fn_affine_3d(void (*fn)(gbMat4* m, union nodearg* args))
{
	assert(def != NULL);
	assert(def->type == TX3D);
	assert(def->tx3d.fn_affine == NULL);
	def->tx3d.fn_affine = fn;
}

static void fn_affine_2d(void (*fn)(gbMat4* m, union nodearg* args))
{
	assert(def != NULL);
	assert(def->type == TX2D);
	assert(def->tx2d.fn_affine == NULL);
	def->tx2d.fn_affine = fn;
}

static void emit_nodedef(void)
{
	// NILNODE must be first
	nodedef(NILNODEDEF, "<NILNODE>");
	nodedef_end();

	/////////////////////////

	nodedef(SDF3D, "sphere");
	arg("radius", RADIUS, 0);
	glsl_sdf3d(
	"float FN(vec3 p, float r)\n"
	"{\n"
	"	return length(p)-r;\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(SDF3D, "box");
	arg("dimensions", DIM3D, 0);
	glsl_sdf3d(
	"float FN(vec3 p, vec3 b)\n"
	"{\n"
	"	vec3 q = abs(p) - b;\n"
	"	return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(SDF2D, "circle");
	arg("radius", RADIUS, 0);
	glsl_sdf2d(
	"float FN(vec2 p, float r)\n"
	"{\n"
	"	return length(p)-r;\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(TX3D, "translate");
	arg("translation", POS3D, 0);
	glsl_tx3d(
	"vec3 FN(vec3 p, vec3 r)\n"
	"{\n"
	"	return p + r;\n"
	"}\n"
	);
	fn_affine_3d(affine_translate_3d);
	nodedef_end();

	/////////////////////////

	nodedef(TX2D, "translate");
	arg("translation", POS2D, 0);
	glsl_tx2d(
	"vec2 FN(vec2 p, vec2 r)\n"
	"{\n"
	"	return p + r;\n"
	"}\n"
	);
	fn_affine_2d(affine_translate_2d);
	nodedef_end();

	/////////////////////////

	nodedef(TX3D, "scale");
	arg("scaling", SCALAR, 0);
	glsl_tx3d(
	"vec3 FN(vec3 p, float s)\n"
	"{\n"
	"	return p / s;\n"
	"}\n"
	);
	glsl_d1(
	"float FN(vec3 _p, float d, float s)\n"
	"{\n"
	"	return d * s;\n"
	"}\n"
	);
	fn_affine_3d(affine_scale_3d);
	nodedef_end();

	/////////////////////////

	nodedef(TX2D, "scale");
	arg("scaling", SCALAR, 0);
	glsl_tx2d(
	"vec2 FN(vec2 p, float s)\n"
	"{\n"
	"	return p / s;\n"
	"}\n"
	);
	glsl_d1(
	"float FN(vec3 _p, float d, float s)\n"
	"{\n"
	"	return d * s;\n"
	"}\n"
	);
	fn_affine_2d(affine_scale_2d);
	nodedef_end();

	/////////////////////////

	nodedef(D1, "round");
	arg("size", SCALAR, 0);
	glsl_d1(
	"float FN(float d, float r)\n"
	"{\n"
	"	return d - r;\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(D1, "onion");
	arg("thickness", SCALAR, 0);
	glsl_d1(
	"float FN(float d, float r)\n"
	"{\n"
	"	return abs(d) - r;\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(D2, "union");
	glsl_d2(
	"float FN(float d0, float d1)\n"
	"{\n"
	"	return min(d0, d1);\n"
	"}\n"
	);
	glsl_d2m(
	"MAT FN(float d0, float d1, MAT m0, MAT m1)\n"
	"{\n"
	"	return d0 < d1 ? m0 : m1;\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(D2, "smooth_union");
	arg("size", SCALAR, 0);
	glsl_d2(
	"float FN(float d0, float d1, float k)\n"
	"{\n"
	"	float h = clamp(0.5 + 0.5*(d1-d0)/k, 0.0, 1.0);\n"
	"	return mix(d1, d0, h) - k*h*(1.0-h);\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(D2, "displace");
	glsl_d2(
	"float FN(float d0, float d1)\n"
	"{\n"
	"	return d0 + d1;\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(VOLUMIZE, "extrude");
	arg("depth", SCALAR, 0);
	glsl_tx(
	"vec2 FN(vec3 p, float h)\n"
	"{\n"
	"	return p.xy;\n"
	"}\n"
	);
	glsl_d1(
	"float FN(vec3 p, float d, float h)\n"
	"{\n"
	"	vec2 w = vec2(d, abs(p.z) - h);\n"
	"	return min(max(w.x,w.y),0.0) + length(max(w,0.0));\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(VOLUMIZE, "revolve");
	arg("offset", SCALAR, 0);
	glsl_tx(
	"vec2 FN(vec3 p, float o)\n"
	"{\n"
	"	return vec2(length(p.xz) - o, p.y);\n"
	"}\n"
	);
	nodedef_end();

	/////////////////////////

	nodedef(TX3D, "elongate");
	arg("extent", DIM3D, 0);
	glsl_tx3d(
	"vec3 FN(vec3 p, vec3 dim)\n"
	"{\n"
	"	return max(abs(p) - dim, 0.0);\n"
	"}\n"
	);
	glsl_d1(
	"vec3 FN(vec3 p, float d, vec3 dim)\n"
	"{\n"
	"	vec3 q = abs(p) - dim;\n"
	"	return min(max(q.x,max(q.y,q.z)),0.0) + d;\n"
	"}\n"
	);
	nodedef_end();

	// TODO "standard" node defs?
	//round_box_2d
	//box_2d
	//oriented_box_2d
	//segment_2d
	//rhombus_2d
	//isosceles_trapezoid_2d
	//parallelogram_2d
	//equiliteral_triangle_2d
	//isosceles_triangle_2d
	//triangle_2d
	//uneven_capsule_2d
	//regular_pentagon_2d
	//regular_hexagon_2d
	//regular_octagon_2d
	//hexagram_2d
	//star5_2d
	//regular_star_2d
	//pie_2d
	//cut_disc_2d
	//arc_2d
	//ring_2d
	//horseshoe_2d
	//vesica_2d
	//oriented_vesica_2d
	//moon_2d
	//circle_cross_2d
	//simple_egg_2d
	//heart_2d
	//cross_2d
	//rounded_x_2d
	//polygon_2d
	//ellipse_2d
	//parabola_2d
	//parabola_segment_2d
	//quadratic_bezier_2d
	//bobbly_cross_2d
	//tunnel_2d
	//stairs_2d
	//hyperbola_2d
	//circle_wave_2d
	//torus_3d
	//capped_torus_3d
	//link_3d
	//infinite_cylinder_3d
	//cone_3d
	//bound_cone_3d
	//infinte_cone_3d
	//plane_3d
	//hexagonal_prism_3d
	//triangular_prism_3d
	//capsule0_3d
	//capsule1_3d
	//capped_cylinder0_3d
	//capped_cylinder1_3d
	//rounded_cylinder_3d
	//capped_cone0_3d
	//capped_cone1_3d
	//solid_angle_3d
	//cut_sphere_3d
	//cut_hollow_sphere_3d
	//death_star_3d
	//round_cone0_3d
	//round_cone1_3d
	//bound_ellipsoid_3d
	//resolved_vesica_3d
	//rhombus_3d
	//octahedron_3d
	//bound_octahedron_3d
	//pyramid_3d
	//triangle_3d
	//quad_3d
	//subtract
	//intersect
	//smooth_subtract
	//smooth_intersect
	//symmetry
	//repeat
	//limit_repeat
	//twist
	//bend
}

static int nodedef_compar(const void* va, const void* vb)
{
	const struct nodedef* a = (const struct nodedef*)va;
	const struct nodedef* b = (const struct nodedef*)vb;
	const int d0 = (int)a->type - (int)b->type;
	if (d0 != 0) return d0;
	return strcmp(a->name, b->name);
}

void nodedef_init(void)
{
	emit_nodedef();
	// enable binary search (via nodedef_find()):
	qsort(nodedefs, n_nodedefs, sizeof nodedefs[0], nodedef_compar);
}

int nodedef_find(enum nodedef_type type, const char* name)
{
	const struct nodedef mock = {
		.type = type,
		.name = name,
	};
	int left = 0;
	int right = n_nodedefs - 1;
	while (left <= right) {
		const int mid = (left + right) >> 1;
		const int d = nodedef_compar(&nodedefs[mid], &mock);
		if (d < 0) {
			left = mid + 1;
		} else if (d > 0) {
			right = mid - 1;
		} else {
			return mid;
		}
	}
	return -1;
}
