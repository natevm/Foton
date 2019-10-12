/* Definitions */
#define PI 3.1415926535897932384626433832795
#define M_PI 3.14159265358979323846
#define M_1_PI 0.318309886183790671538
#define TWOPI 6.283185307
#define TWO_PI TWOPI
#define E 2.71828182845904523536f
#define EPSILON 0.001
#define INFINITY 1000000.

#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_RECTANGLE 1
#define LIGHT_TYPE_DISK 2
#define LIGHT_TYPE_ROD 3
#define LIGHT_TYPE_SPHERE 4

// #define USE_MIS
#define MAX_BOUNCES 5

#define USE_LTC true

#define ALPHA_MINIMUM .01

#define TMIN .01
// #define TMAX 1000 /*1e20f*/
#define TMAX 1000

#define MOTION_VECTOR_OFFSET vec2(.5, .5)
#define MOTION_VECTOR_MIN .0

#define MAX_TRANSPARENCY_CONTINUES 10

#define MIN_SAMPLES 2
#define MAX_CUMULATIVE_COUNT 128
#define TEMPORAL_ALPHA (1.0 / float(MAX_CUMULATIVE_COUNT))

// The larger max cumulative count is, the more powerful soft reset rate should be?
#define MIN_SOFT_RESET_COUNT 4
#define SOFT_RESET_RATE .2
#define DIRECTION_WEIGHT 1.0
#define DISTANCE_MAX .1
#define POSITION_MAX 1.0
#define ALBEDO_MAX .1
#define NORMAL_WEIGHT 1.0
#define ENTITY_WEIGHT 1.0
#define GRADIENT_WEIGHT 0.5

#define GRADIENT_ALPHA 0.9
#define MAX_ERROR 10.

// used during TAA
#define GRADIENT_FILTER_RADIUS 2

#define PATH_TRACE_TILE_SIZE 3
#define GRADIENT_TILE_SIZE 3
