float Hash(int pixel_i, int pixel_j, float seed, int dimension){
    float phi = 1.61803398874989484820459 * 00000.1; // Golden Ratio
    float pi  = 3.14159265358979323846264 * 00000.1; // pi
    float sq2 = 1.41421356237309504880169 * 10000.0; // Square Root of Two
    int size = push.consts.width * push.consts.height;

    return (2.0 * fract(sin(dot(vec2(float(pixel_i)/push.consts.width + dimension, float(pixel_j)/push.consts.height + dimension)*(seed+phi), vec2(phi, pi)))*sq2)) - 1.0;
}

float Halton(int b, int i)
{
    float r = 0.0;
    float f = 1.0;
    while (i > 0) {
        f = f / float(b);
        r = r + f * float(i % b);
        i = int(floor(float(i) / float(b)));
    }
    return r;
}

float Halton(int base, int pixel_i, int pixel_j, int frameIndex)
{
    // pixel_i = pixel_i & 127;
    // pixel_j = pixel_j & 127;
    // frameIndex = frameIndex + pixel_i + pixel_j * 128;
    frameIndex = frameIndex & 255;

    float r = 0.0;
    float f = 1.0;
    while (frameIndex > 0) {
        f = f / float(base);
        r = r + f * float(frameIndex % base);
        frameIndex = int(floor(float(frameIndex) / float(base)));
    }
    return r;
}

float Halton2(int i)
{
#if __VERSION__ >= 400
	return float(bitfieldReverse(uint(i)))/4294967296.0;
#else
	return Halton(2, i);
#endif
}

vec2 Halton23(int i)
{
    return vec2(Halton2(i), Halton(3, i));
}