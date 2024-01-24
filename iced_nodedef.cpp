#include <assert.h>

#include "iced_nodedef.h"


static bool in_node = false;


static void node(const char* name)
{
	assert(!in_node);
	in_node = true;
	// TODO
}

static void endnode(void)
{
	assert(in_node);
	in_node = false;
	// TODO
}

static void sdf3d_glsl(const char* src)
{
	assert(in_node);
	// TODO
}

static void sdf2d_glsl(const char* src)
{
	assert(in_node);
	// TODO
}

static void radius(int argidx)
{
	assert(in_node);
	// TODO
}

static void dim3(int argidx)
{
	assert(in_node);
	// TODO
}

void nodedef_init(void)
{
	node("sphere_3d");
	radius(1);
	sdf3d_glsl(
	"float FN(vec3 p, float r)\n"
	"{\n"
	"	return length(p)-r;\n"
	"}\n"
	);
	endnode();

	node("box_3d");
	dim3(1);
	sdf3d_glsl(
	"float FN(vec3 p, vec3 b)\n"
	"{\n"
	"	vec3 q = abs(p) - b;\n"
	"	return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);\n"
	"}\n"
	);
	endnode();

	node("circle_2d");
	radius(1);
	sdf2d_glsl(
	"float FN(vec2 p, float r)\n"
	"{\n"
	"	return length(p)-r;\n"
	"}\n"
	);
	endnode();

	// TODO
	//procedure
	//set_material
	//affine_2d
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
	//affine_3d
	//torus_3d
	//capped_torus_3d
	//link_3d
	//infinte_cylinder_3d
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
	//revolve
	//extrude
	//elongate0
	//elongate1
	//round
	//onion
	//union
	//subtract
	//intersect
	//smooth_union
	//smooth_subtract
	//smooth_intersect
	//symmetry
	//repeat
	//limit_repeat
	//displace
	//twist
	//bend
	//raymarch
	//render
	//optimize
	//navmesh_gen
	//entity
}
