#ifndef OPTIONS
#define OPTIONS

bool is_multiview_enabled()
{
    return (push.consts.flags & (1 << 0)) == 0;
}

bool is_taa_enabled()
{
    return (push.consts.flags & (1 << 2)) != 0;
}

bool is_progressive_refinement_enabled()
{
    return (push.consts.flags & (1 << 6)) != 0;
}

bool is_atrous_enabled()
{
    return (push.consts.flags & (1 << 3)) != 0;
}

bool show_bounding_box()
{
    return (push.consts.flags & (1 << 4)) != 0;
}


bool is_reverse_z_enabled()
{
    return (push.consts.flags & (1 << 1)) == 0;
}

bool is_blue_noise_enabled()
{
    return (push.consts.flags & (1 << 5)) != 0;
}

#endif