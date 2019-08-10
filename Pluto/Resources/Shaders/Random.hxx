#ifndef BLUE_NOISE
#define BLUE_NOISE

#include "Pluto/Resources/Shaders/Options.hxx"

float seed;


#define HASHSCALE1 .1031
float hash12(vec2 p) {
    vec3 p3  = fract(vec3(p.xyx) * HASHSCALE1);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

int sampleRankingTile(ivec2 coordinates)
{
    if ((push.consts.ranking_tile_id < 0) || (push.consts.ranking_tile_id >= MAX_TEXTURES))
        return 0;

    TextureStruct tex = txbo.textures[push.consts.ranking_tile_id];

    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_SAMPLERS)) 
        return 0;

	return int(texelFetch( 
			sampler2D(texture_2Ds[push.consts.ranking_tile_id], samplers[tex.sampler_id]), 
			coordinates, 0
    ).r * 255.5);
}

int sampleSobelTile(ivec2 coordinates)
{
    if ((push.consts.sobel_tile_id < 0) || (push.consts.sobel_tile_id >= MAX_TEXTURES))
        return 0;

    TextureStruct tex = txbo.textures[push.consts.sobel_tile_id];

    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_SAMPLERS)) 
        return 0;

    return int(texelFetch(sampler2D(texture_2Ds[push.consts.sobel_tile_id], samplers[tex.sampler_id]), 
        coordinates, 0
    ).r * 255.5);

    // return int(texture(texture_2Ds[push.consts.sobel_tile_id], uv) * 255.0);
	// return int(texture( 
	// 		sampler2D(texture_2Ds[push.consts.sobel_tile_id], samplers[tex.sampler_id]), 
	// 		uv
    // ).r * 255.0);
}

int sampleScramblingTile(ivec2 coordinates)
{
    if ((push.consts.scramble_tile_id < 0) || (push.consts.scramble_tile_id >= MAX_TEXTURES))
        return 0;

    TextureStruct tex = txbo.textures[push.consts.scramble_tile_id];

    if ((tex.sampler_id < 0) || (tex.sampler_id >= MAX_SAMPLERS)) 
        return 0;

	return int(texelFetch( 
			sampler2D(texture_2Ds[push.consts.scramble_tile_id], samplers[tex.sampler_id]), 
			coordinates, 0
    ).r * 255.5);
}

float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(int pixel_i, int pixel_j, int sampleIndex, int sampleDimension)
{
	pixel_i = pixel_i & 127;
	pixel_j = pixel_j & 127;
	sampleIndex = sampleIndex & 255;
	sampleDimension = sampleDimension & 255;

    int rankingTileAddr = sampleDimension + (pixel_i + pixel_j*128)*8;
    int rankingTileAddrX = rankingTileAddr % 128;
    int rankingTileAddrY = rankingTileAddr / 128;

	// xor index based on optimized ranking
	int rankedSampleIndex = sampleIndex ^ sampleRankingTile(ivec2(rankingTileAddrX, rankingTileAddrY));

    int sobelTileAddr = sampleDimension + rankedSampleIndex*256;
    int sobelTileAddrX = sobelTileAddr % 256;
    int sobelTileAddrY = sobelTileAddr / 256;

	// fetch value in sequence
	int value = sampleSobelTile(ivec2(sampleDimension, rankedSampleIndex)); //sobol_256spp_256d[sampleDimension + rankedSampleIndex*256];

    int scramblingTileAddr = (sampleDimension%8) + (pixel_i + pixel_j*128)*8;
    int scramblingTileAddrX = scramblingTileAddr % 128;
    int scramblingTileAddrY = scramblingTileAddr / 128;

	// If the dimension is optimized, xor sequence value based on optimized scrambling
	value = value ^ sampleScramblingTile(ivec2(scramblingTileAddrX, scramblingTileAddrY)); //scramblingTile[(sampleDimension%8) + (pixel_i + pixel_j*128)*8];

	// convert to float and return
	float v = (0.5f+value)/255.0f;
	return v;
}

int randomDimension;


// http://www.pcg-random.org/download.html
struct PCGRand {
	uint64_t state;
	// Just use stream 1
};

int pcg32_random(inout PCGRand rng) {
	uint64_t oldstate = rng.state;
	rng.state = oldstate * uint64_t(6364136223846793005UL) + 1;
	// Calculate output function (XSH RR), uses old state for max ILP
	int xorshifted = int(((oldstate >> 18u) ^ oldstate) >> 27u);
	int rot = int(oldstate >> 59u);
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

float pcg32_randomf(inout PCGRand rng) {
	return float(ldexp(double(pcg32_random(rng)), -32));
}

PCGRand get_rng(ivec2 pixel, int seed) {
	PCGRand rng;
	rng.state = 0;
	pcg32_random(rng);
	rng.state += seed;
	pcg32_random(rng);
	return rng;
}


// PCGRand rng;
// rng.state = uint64_t(uint64_t(pixel.x + pixel.y * push.consts.width) + uint64_t(push.consts.width * push.consts.height * randomDimension)) * uint64_t(push.consts.frame + 1);
// return pcg32_randomf(rng);

float random(ivec2 pixel) {
    randomDimension++;
    if (is_blue_noise_enabled() && (!is_progressive_refinement_enabled() || (push.consts.frame < 256)) ) {
        return samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_128spp(pixel.x, pixel.y, push.consts.frame, randomDimension);
    } else {
        return fract(sin(randomDimension + hash12( pixel ) + push.consts.frame) * 43758.5453123);
    }
//    
}

#endif