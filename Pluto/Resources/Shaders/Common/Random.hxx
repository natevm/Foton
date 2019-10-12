#ifndef RANDOM_HXX
#define RANDOM_HXX

#extension GL_ARB_gpu_shader_int64 : enable
#define uint32_t uint

#include "Pluto/Resources/Shaders/Common/Options.hxx"


// http://www.pcg-random.org/download.html
struct PCGRand {
	uint64_t state;
	// Just use stream 1
};

uint32_t pcg32_random(inout PCGRand rng) {
	uint64_t oldstate = rng.state;
	rng.state = oldstate * 6364136223846793005UL + 1;
	// Calculate output function (XSH RR), uses old state for max ILP
	uint32_t xorshifted = uint32_t(((oldstate >> 18u) ^ oldstate) >> 27u);
	uint32_t rot = uint32_t(oldstate >> 59u);
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

float pcg32_randomf(inout PCGRand rng) {
	return float(ldexp(double(pcg32_random(rng)), -32));
}

PCGRand get_rng(ivec2 pixel, int frame_id, int seed) {
    uvec2 p = uvec2(pixel.xy);
	uint32_t s = (p.x + p.y * push.consts.width) * (frame_id + 1 + seed);

	PCGRand rng;
	rng.state = 0;
	pcg32_random(rng);
	rng.state += s;
	pcg32_random(rng);
	return rng;
}


float seed;
#define HASHSCALE1 .1031
float hash12(vec2 p) {
    vec3 p3  = fract(vec3(p.xyx) * HASHSCALE1);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

// int sampleRankingTile(ivec2 coordinates)
// {
//     if ((push.consts.ranking_tile_id < 0) || (push.consts.ranking_tile_id >= max_textures))
//         return 0;

//     TextureStruct tex = txbo.textures[push.consts.ranking_tile_id];

//     if ((tex.sampler_id < 0) || (tex.sampler_id >= max_samplers)) 
//         return 0;

// 	return int(texelFetch( 
// 			sampler2D(texture_2Ds[push.consts.ranking_tile_id], samplers[tex.sampler_id]), 
// 			coordinates, 0
//     ).r * 255.5);
// }

// int sampleSobelTile(ivec2 coordinates)
// {
//     if ((push.consts.sobel_tile_id < 0) || (push.consts.sobel_tile_id >= max_textures))
//         return 0;

//     TextureStruct tex = txbo.textures[push.consts.sobel_tile_id];

//     if ((tex.sampler_id < 0) || (tex.sampler_id >= max_samplers)) 
//         return 0;

//     return int(texelFetch(sampler2D(texture_2Ds[push.consts.sobel_tile_id], samplers[tex.sampler_id]), 
//         coordinates, 0
//     ).r * 255.5);

//     // return int(texture(texture_2Ds[push.consts.sobel_tile_id], uv) * 255.0);
// 	// return int(texture( 
// 	// 		sampler2D(texture_2Ds[push.consts.sobel_tile_id], samplers[tex.sampler_id]), 
// 	// 		uv
//     // ).r * 255.0);
// }

// int sampleScramblingTile(ivec2 coordinates)
// {
//     if ((push.consts.scramble_tile_id < 0) || (push.consts.scramble_tile_id >= max_textures))
//         return 0;

//     TextureStruct tex = txbo.textures[push.consts.scramble_tile_id];

//     if ((tex.sampler_id < 0) || (tex.sampler_id >= max_samplers)) 
//         return 0;

// 	return int(texelFetch( 
// 			sampler2D(texture_2Ds[push.consts.scramble_tile_id], samplers[tex.sampler_id]), 
// 			coordinates, 0
//     ).r * 255.5);
// }

// // Belcour sampler
// float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(int pixel_i, int pixel_j, int sampleIndex, int sampleDimension)
// {
// 	pixel_i = pixel_i & 127;
// 	pixel_j = pixel_j & 127;
// 	sampleIndex = sampleIndex & 255;
// 	sampleDimension = sampleDimension & 255;

//     int rankingTileAddr = sampleDimension + (pixel_i + pixel_j*128)*8;
//     int rankingTileAddrX = rankingTileAddr % 128;
//     int rankingTileAddrY = rankingTileAddr / 128;

// 	// xor index based on optimized ranking
// 	int rankedSampleIndex = sampleIndex ^ sampleRankingTile(ivec2(rankingTileAddrX, rankingTileAddrY));

//     int sobelTileAddr = sampleDimension + rankedSampleIndex*256;
//     int sobelTileAddrX = sobelTileAddr % 256;
//     int sobelTileAddrY = sobelTileAddr / 256;

// 	// fetch value in sequence
// 	int value = sampleSobelTile(ivec2(sampleDimension, rankedSampleIndex)); //sobol_256spp_256d[sampleDimension + rankedSampleIndex*256];

//     int scramblingTileAddr = (sampleDimension%8) + (pixel_i + pixel_j*128)*8;
//     int scramblingTileAddrX = scramblingTileAddr % 128;
//     int scramblingTileAddrY = scramblingTileAddr / 128;

// 	// If the dimension is optimized, xor sequence value based on optimized scrambling
// 	value = value ^ sampleScramblingTile(ivec2(scramblingTileAddrX, scramblingTileAddrY)); //scramblingTile[(sampleDimension%8) + (pixel_i + pixel_j*128)*8];

// 	// convert to float and return
// 	float v = (0.5f+value)/255.0f;
// 	return v;
// }

int randomDimension;

// Simple blue noise tile
float sampleBlueNoiseTile(ivec2 pixel_seed, int frame_seed, int random_dimension) {
    // Note, sampler repeats
    int color_dim = ((frame_seed + random_dimension) % 4);
    int tile_dim = ((frame_seed + random_dimension) / 4) % 64;
    pixel_seed.x = pixel_seed.x % 64;
    pixel_seed.y = pixel_seed.y % 64;
    vec4 val = texelFetch ( 
        sampler3D(BlueNoiseTile, samplers[1]), 
        ivec3(pixel_seed.x, pixel_seed.y, tile_dim), 
        0
    );
    return val[color_dim];
}

// PCGRand rng;
// rng.state = uint64_t(uint64_t(pixel.x + pixel.y * push.consts.width) + uint64_t(push.consts.width * push.consts.height * randomDimension)) * uint64_t(push.consts.frame + 1);
// return pcg32_randomf(rng);
PCGRand rng;
ivec2 blue_noise_pixel_seed; 
int blue_noise_frame_seed;
int blue_noise_random_dimension;
void init_random(ivec2 pixel_seed, int frame_seed, int random_dimension)
{
    randomDimension = random_dimension;
    blue_noise_pixel_seed = pixel_seed;
    blue_noise_frame_seed = frame_seed;
    blue_noise_random_dimension = random_dimension;
    rng = get_rng(pixel_seed, frame_seed, random_dimension);
}

float random() {
    randomDimension++;
    // return samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(pixel_seed.x, pixel_seed.y, frame_seed, randomDimension);
    if (is_blue_noise_enabled() && (!is_progressive_refinement_enabled() || (blue_noise_frame_seed < 64 * 4)) ) {
        return sampleBlueNoiseTile(blue_noise_pixel_seed, blue_noise_frame_seed, randomDimension);
    } else {
        return pcg32_randomf(rng);
    }
}

#endif