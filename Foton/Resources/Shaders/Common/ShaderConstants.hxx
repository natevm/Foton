/* Definitions */
#define PI 3.1415926535897932384626433832795
#define M_PI 3.14159265358979323846
#define M_1_PI 0.318309886183790671538
#define TWOPI 6.283185307
#define TWO_PI TWOPI
#define E 2.71828182845904523536f
#define EPSILON 0.001
#define INFINITY 1000000.

// #define USE_MIS
#define MAX_BOUNCES 5

#define USE_LTC true

#define ALPHA_MINIMUM .01
#define CLEARCOAT_ALPHA_MINIMUM .001

#define TMIN .001
// #define TMIN .0
// #define TMAX 1000 /*1e20f*/
#define TMAX 1000

#define MOTION_VECTOR_OFFSET vec2(.5, .5)
#define MOTION_VECTOR_MIN .0

#define MAX_TRANSPARENCY_CONTINUES 10

// Min samples increases temporal coherence during disocclusions, 
// but introduces artifacts due to temporal gradient not properly causing sample reset
#define MIN_SAMPLES 0
#define MAX_DIFFUSE_CUMULATIVE_COUNT 16
#define MIN_DIFFUSE_CUMULATIVE_COUNT 1
#define TEMPORAL_ALPHA (1.0 / float(MAX_DIFFUSE_CUMULATIVE_COUNT))

// Specular is much more likely to ghost, due to more undefined disocclusions.
#define MAX_SPECULAR_CUMULATIVE_COUNT 16
#define MIN_SPECULAR_CUMULATIVE_COUNT 2

// The larger max cumulative count is, the more powerful soft reset rate should be?
#define MIN_SOFT_RESET_COUNT 4
#define SOFT_RESET_RATE .2
#define DIRECTION_WEIGHT 1.0
#define DISTANCE_MAX .1
#define POSITION_MAX 1.0
#define ALBEDO_MAX .1
#define NORMAL_WEIGHT 1.0
#define ENTITY_WEIGHT 1.0
// would be cool to have this as a parameter, or figure out how to get rid of it.
// #define GRADIENT_WEIGHT 1.1 

#define GRADIENT_ALPHA 1.0
#define MAX_ERROR 10.

// used during TAA
// #define GRADIENT_FILTER_RADIUS 8
#define GRADIENT_FILTER_RADIUS 4
// #define GRADIENT_DOWNSAMPLE 1.0

// Have a feeling allowing tile size to change at runtime hurts
// unroll and jam performance
// (push.consts.path_trace_tile_size)
// Adding path trace tile size to gradient tile size instead
// of multiplying, since multiplying makes certain convolutions
// too expensive. 
#define PATH_TRACE_TILE_SIZE 2
#define GRADIENT_TILE_SIZE (2 + PATH_TRACE_TILE_SIZE)

#define GRADIENT_DOWNSAMPLE (1.0/float(10))
#define GRADIENT_USE_RANDOM false
#define GRADIENT_MAX_PIXEL_DISTANCE 5

#define PDF_CLAMP .1