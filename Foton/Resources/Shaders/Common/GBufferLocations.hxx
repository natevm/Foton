/* G buffer locations */

// Data passes
#define POSITION_DEPTH_ADDR 0 // NOTE, must be in position 0 for shadowmap raster pass!
#define ENTITY_MATERIAL_TRANSFORM_LIGHT_ADDR 1
#define NORMAL_ADDR 2
#define SEED_ADDR 3 // consider separating luminance from seed...
#define UV_METALLIC_ROUGHESS_ADDR 6 // consider replacing with "material ID"

#define ENTITY_MATERIAL_TRANSFORM_LIGHT_ADDR_PREV 7
#define POSITION_DEPTH_ADDR_PREV 8
#define NORMAL_ADDR_PREV 9
#define UV_METALLIC_ROUGHESS_ADDR_PREV 10
#define SEED_ADDR_PREV 11
#define DIFFUSE_MOTION_ADDR 5
#define GLOSSY_MOTION_ADDR 12

#define DIFFUSE_SEED_ADDR 3
#define DIFFUSE_SEED_ADDR_PREV 11
#define SPECULAR_SEED_ADDR 57
#define SPECULAR_SEED_ADDR_PREV 56


#define LOBE_AXIS_SHARPNESS_ADDR 41
#define LOBE_AXIS_SHARPNESS_ADDR_PREV 42
#define REFLECTED_ID_ADDR_AND_PREV 58

// Lighting passes
#define DIFFUSE_COLOR_ADDR 4
#define DIFFUSE_COLOR_ADDR_PREV 15
#define GLOSSY_COLOR_ADDR 46
#define GLOSSY_COLOR_ADDR_PREV
#define TRANSMISSION_COLOR_ADDR 
#define TRANSMISSION_COLOR_PREV 
#define EMISSION_ADDR 43
#define EMISSION_ADDR_PREV 
#define ENVIRONMENT_ADDR 47
#define ENVIRONMENT_ADDR_PREV 
#define DIFFUSE_DIRECT_ADDR 49
#define DIFFUSE_INDIRECT_ADDR 48
#define GLOSSY_DIRECT_ADDR 45
#define GLOSSY_INDIRECT_ADDR 44

#define COARSE_NORMAL_ADDR 50
#define COARSE_POSITION_DEPTH_ADDR 51

// Denoising
// Raytracing buffers
        // might be able to get rid of these...
#define SAMPLE_COUNT_ADDR 18
#define SAMPLE_COUNT_ADDR_PREV 19

// Progressive refinement buffers
#define PROGRESSIVE_HISTORY_ADDR 20

// Atrous iterative buffers
#define ATROUS_HISTORY_1_ADDR 21
#define ATROUS_HISTORY_2_ADDR 22
#define ATROUS_HISTORY_3_ADDR 23 // DONT NEED THESE?
#define ATROUS_HISTORY_4_ADDR 16 // DONT NEED THESE?

// Temporal gradient buffers
// TODO, remove LUM_VAR_ADDRs
#define TEMPORAL_GRADIENT_ADDR 24
#define LUMINANCE_VARIANCE_ADDR 26
#define LUMINANCE_VARIANCE_ADDR_PREV 27
#define LUMINANCE_AVERAGE_ADDR 25
// #define VARIANCE_ADDR 28
#define LUMINANCE_ADDR  13
#define LUMINANCE_ADDR_PREV  14
#define LUMINANCE_MAX_ADDR  17

// TAA history buffers
#define TAA_HISTORY_1_ADDR 29
#define TAA_HISTORY_2_ADDR 30
#define SVGF_TAA_HISTORY_1_ADDR 31
#define SVGF_TAA_HISTORY_2_ADDR 32
#define SVGF_TAA_HISTORY_3_ADDR 33
#define SVGF_TAA_HISTORY_4_ADDR 34
#define SVGF_TAA_HISTORY_5_ADDR 35
#define SVGF_TAA_HISTORY_6_ADDR 36
#define SVGF_TAA_HISTORY_7_ADDR 37
#define SVGF_TAA_HISTORY_8_ADDR 38
#define SVGF_TAA_HISTORY_9_ADDR 39
#define SVGF_TAA_HISTORY_10_ADDR 40


#define RANDOM_ADDR 59