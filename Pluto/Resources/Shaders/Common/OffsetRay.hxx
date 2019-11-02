#ifndef OFFSET_RAY_HXX
#define OFFSET_RAY_HXX

#define OFFSET_RAY_ORIGIN (1.0f / 32.0f)
#define OFFSET_RAY_FLOAT_SCALE (1.0f / 65536.0f)
#define OFFSET_RAY_INT_SCALE (256.0f)

// Normal points outward for rays exiting the surface, else is flipped. 
vec3 offset_ray(const vec3 p, const vec3 n) 
{ 
    return p;

    // ivec3 of_i = ivec3(OFFSET_RAY_INT_SCALE * n.x, OFFSET_RAY_INT_SCALE * n.y, OFFSET_RAY_INT_SCALE * n.z); 
    
    // vec3 p_i = vec3( 
    //     intBitsToFloat(floatBitsToInt(p.x)+((p.x < 0) ? -of_i.x : of_i.x)), 
    //     intBitsToFloat(floatBitsToInt(p.y)+((p.y < 0) ? -of_i.y : of_i.y)), 
    //     intBitsToFloat(floatBitsToInt(p.z)+((p.z < 0) ? -of_i.z : of_i.z))); 
    
    // return vec3(abs(p.x) < OFFSET_RAY_ORIGIN ? p.x + OFFSET_RAY_FLOAT_SCALE*n.x : p_i.x, 
    //             abs(p.y) < OFFSET_RAY_ORIGIN ? p.y + OFFSET_RAY_FLOAT_SCALE*n.y : p_i.y, 
    //             abs(p.z) < OFFSET_RAY_ORIGIN ? p.z + OFFSET_RAY_FLOAT_SCALE*n.z : p_i.z); 
}

#endif