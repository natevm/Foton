#ifndef OPTIONS
#define OPTIONS

#define RASTERIZE_MULTIVIEW 0
#define DISABLE_REVERSE_Z_PROJECTION 1
#define ENABLE_TAA 2
#define ENABLE_ASVGF_GRADIENT 3
#define ENABLE_ASVGF_GRADIENT_ATROUS 15
#define ENABLE_ASVGF_TEMPORAL_ACCUMULATION 11
#define ENABLE_ASVGF_VARIANCE_ESTIMATION 12
#define ENABLE_ASVGF_ATROUS 13
#define ENABLE_ASVGF 14
#define RASTERIZE_BOUNDING_BOX 4
#define ENABLE_BLUE_NOISE 5
#define ENABLE_PROGRESSIVE_REFINEMENT 6
#define ENABLE_SVGF_TAA 7
#define SHOW_DIRECT_ILLUMINATION 8
#define SHOW_INDIRECT_ILLUMINATION 9
#define SHOW_ALBEDO 10
#define ENABLE_ANALYICAL_AREA_LIGHTS 16
#define ENABLE_BILATERAL_UPSAMPLING 17

bool is_multiview_enabled()
{
    return (push.consts.flags & (1 << RASTERIZE_MULTIVIEW)) == 0;
}

bool is_taa_enabled()
{
    return (push.consts.flags & (1 << ENABLE_TAA)) != 0;
}

bool is_progressive_refinement_enabled()
{
    return (push.consts.flags & (1 << ENABLE_PROGRESSIVE_REFINEMENT)) != 0;
}

bool is_asvgf_atrous_enabled()
{
    return (push.consts.flags & (1 << ENABLE_ASVGF_ATROUS)) != 0;
}

bool show_bounding_box()
{
    return (push.consts.flags & (1 << RASTERIZE_BOUNDING_BOX)) != 0;
}


bool is_reverse_z_enabled()
{
    return (push.consts.flags & (1 << DISABLE_REVERSE_Z_PROJECTION)) == 0;
}

bool is_blue_noise_enabled()
{
    return (push.consts.flags & (1 << ENABLE_BLUE_NOISE)) != 0;
}

bool is_ltc_enabled()
{
    return (push.consts.flags & (1 << ENABLE_ANALYICAL_AREA_LIGHTS)) != 0;
}

bool is_gradient_estimation_enabled()
{
    return (push.consts.flags & (1 << ENABLE_ASVGF_GRADIENT)) != 0;
}

bool should_show_direct_illumination()
{
    return (push.consts.flags & (1 << SHOW_DIRECT_ILLUMINATION)) != 0;
}

bool should_show_indirect_illumination()
{
    return (push.consts.flags & (1 << SHOW_INDIRECT_ILLUMINATION)) != 0;
}

bool should_show_albedo()
{
    return (push.consts.flags & (1 << SHOW_ALBEDO)) != 0;
}


bool is_bilateral_upsampling_enabled()
{
    return (push.consts.flags & (1 << ENABLE_BILATERAL_UPSAMPLING)) != 0;
}

#endif