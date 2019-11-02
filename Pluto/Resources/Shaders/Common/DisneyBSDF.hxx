#ifndef DISNEY_BSDF_GLSL
#define DISNEY_BSDF_GLSL

// #include "util.hlsl"
#include "Pluto/Resources/Shaders/Common/Utilities.hxx"
#include "Pluto/Resources/Shaders/Common/ShaderConstants.hxx"
#include "Pluto/Resources/Shaders/Common/Random.hxx"
#include "Pluto/Material/MaterialStruct.hxx"

/* Disney BSDF functions, for additional details and examples see:
 * - https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
 * - https://www.shadertoy.com/view/XdyyDd
 * - https://github.com/wdas/brdf/blob/master/src/brdfs/disney.brdf
 * - https://schuttejoe.github.io/post/disneybsdf/
 *
 * Variable naming conventions with the Burley course notes:
 * V -> w_o
 * L -> w_i
 * H -> w_h
 */


bool same_hemisphere(const in vec3 w_o, const in vec3 w_i, const in vec3 n) {
	return dot(w_o, n) * dot(w_i, n) > 0.f;
}

// Sample the hemisphere using a cosine weighted distribution,
// returns a vector in a hemisphere oriented about (0, 0, 1)
vec3 cos_sample_hemisphere(vec2 u) {
	vec2 s = 2.f * u - 1.f;
	vec2 d;
	float radius = 0;
	float theta = 0;
	if (s.x == 0.f && s.y == 0.f) {
		d = s;
	} else {
		if (abs(s.x) > abs(s.y)) {
			radius = s.x;
			theta  = M_PI / 4.f * (s.y / s.x);
		} else {
			radius = s.y;
			theta  = M_PI / 2.f - M_PI / 4.f * (s.x / s.y);
		}
	}
	d = radius * vec2(cos(theta), sin(theta));
	return vec3(d.x, d.y, sqrt(max(0.f, 1.f - d.x * d.x - d.y * d.y)));
}

vec3 spherical_dir(float sin_theta, float cos_theta, float phi) {
	return vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

float power_heuristic(float n_f, float pdf_f, float n_g, float pdf_g) {
	float f = n_f * pdf_f;
	float g = n_g * pdf_g;
	return (f * f) / (f * f + g * g);
}

float schlick_weight(float cos_theta) {
	return pow(saturate(1.f - cos_theta), 5.f);
}

// Complete Fresnel Dielectric computation, for transmission at ior near 1
// they mention having issues with the Schlick approximation.
// eta_i: material on incident side's ior
// eta_t: material on transmitted side's ior
float fresnel_dielectric(float cos_theta_i, float eta_i, float eta_t) {
	float g = pow2(eta_t) / pow2(eta_i) - 1.f + pow2(cos_theta_i);
	if (g < 0.f) {
		return 1.f;
	}
	return 0.5f * pow2(g - cos_theta_i) / pow2(g + cos_theta_i)
		* (1.f + pow2(cos_theta_i * (g + cos_theta_i) - 1.f) / pow2(cos_theta_i * (g - cos_theta_i) + 1.f));
}

// D_GTR1: Generalized Trowbridge-Reitz with gamma=1
// Burley notes eq. 4
float gtr_1(float cos_theta_h, float alpha) {
	if (alpha >= 1.f) {
		return M_1_PI;
	}
	float alpha_sqr = alpha * alpha;
	return M_1_PI * (alpha_sqr - 1.f) / (log(alpha_sqr) * (1.f + (alpha_sqr - 1.f) * cos_theta_h * cos_theta_h));
}

// D_GTR2: Generalized Trowbridge-Reitz with gamma=2
// Burley notes eq. 8
float gtr_2(float cos_theta_h, float alpha) {
	float alpha_sqr = alpha * alpha;
	return M_1_PI * alpha_sqr / pow2(1.f + (alpha_sqr - 1.f) * cos_theta_h * cos_theta_h);
}

// D_GTR2 Anisotropic: Anisotropic generalized Trowbridge-Reitz with gamma=2
// Burley notes eq. 13
float gtr_2_aniso(float h_dot_n, float h_dot_x, float h_dot_y, vec2 alpha) {
	return M_1_PI / (alpha.x * alpha.y
			* pow2(pow2(h_dot_x / alpha.x) + pow2(h_dot_y / alpha.y) + h_dot_n * h_dot_n));
}

float smith_shadowing_ggx(float n_dot_o, float alpha_g) {
	float a = alpha_g * alpha_g;
	float b = n_dot_o * n_dot_o;
	return 1.f / (n_dot_o + sqrt(a + b - a * b));
}

float smith_shadowing_ggx_aniso(float n_dot_o, float o_dot_x, float o_dot_y, vec2 alpha) {
	return 1.f / (n_dot_o + sqrt(pow2(o_dot_x * alpha.x) + pow2(o_dot_y * alpha.y) + pow2(n_dot_o)));
}

// Sample a reflection direction the hemisphere oriented along n and spanned by v_x, v_y using the random samples in s
vec3 sample_lambertian_dir(const in vec3 n, const in vec3 v_x, const in vec3 v_y, const in vec2 s) {
	const vec3 hemi_dir = normalize(cos_sample_hemisphere(s));
	return hemi_dir.x * v_x + hemi_dir.y * v_y + hemi_dir.z * n;
}

// Sample the microfacet normal vectors for the various microfacet distributions
vec3 sample_gtr_1_h(const in vec3 n, const in vec3 v_x, const in vec3 v_y, float alpha, const in vec2 s) {
	float phi_h = 2.f * M_PI * s.x;
	float alpha_sqr = alpha * alpha;
	float cos_theta_h_sqr = (1.f - pow(alpha_sqr, 1.f - s.y)) / (1.f - alpha_sqr);
	float cos_theta_h = sqrt(cos_theta_h_sqr);
	float sin_theta_h = 1.f - cos_theta_h_sqr;
	vec3 hemi_dir = normalize(spherical_dir(sin_theta_h, cos_theta_h, phi_h));
	return hemi_dir.x * v_x + hemi_dir.y * v_y + hemi_dir.z * n;
}

vec3 sample_gtr_2_h(const in vec3 n, const in vec3 v_x, const in vec3 v_y, float alpha, const in vec2 s) {
	float phi_h = 2.f * M_PI * s.x;
	float cos_theta_h_sqr = (1.f - s.y) / (1.f + (alpha * alpha - 1.f) * s.y);
	float cos_theta_h = sqrt(cos_theta_h_sqr);
	float sin_theta_h = 1.f - cos_theta_h_sqr;
	vec3 hemi_dir = normalize(spherical_dir(sin_theta_h, cos_theta_h, phi_h));
	return hemi_dir.x * v_x + hemi_dir.y * v_y + hemi_dir.z * n;
}

vec3 sample_gtr_2_aniso_h(const in vec3 n, const in vec3 v_x, const in vec3 v_y, const in vec2 alpha, const in vec2 s) {
	float x = 2.f * M_PI * s.x;
	vec3 w_h = sqrt(s.y / (1.f - s.y)) * (alpha.x * cos(x) * v_x + alpha.y * sin(x) * v_y) + n;
	return normalize(w_h);
}

float lambertian_pdf(const in vec3 w_i, const in vec3 n) {
	float d = dot(w_i, n);
	if (d > 0.f) {
		return d * M_1_PI;
	}
	return 0.f;
}

float gtr_1_pdf(const in vec3 w_o, const in vec3 w_i, const in vec3 n, float alpha) {
	if (!same_hemisphere(w_o, w_i, n)) {
		return 0.f;
	}
	vec3 w_h = normalize(w_i + w_o);
	float cos_theta_h = dot(n, w_h);
	float d = gtr_1(cos_theta_h, alpha);
	return d * cos_theta_h / (4.f * dot(w_o, w_h));
}

float gtr_2_pdf(const in vec3 w_o, const in vec3 w_i, const in vec3 n, float alpha) {
	if (!same_hemisphere(w_o, w_i, n)) {
		return 0.f;
	}
	vec3 w_h = normalize(w_i + w_o);
	float cos_theta_h = dot(n, w_h);
	float d = gtr_2(cos_theta_h, alpha);
	return d * cos_theta_h / (4.f * dot(w_o, w_h));
}

float gtr_2_transmission_pdf(const in vec3 w_o, const in vec3 w_i, const in vec3 n,
	float alpha, float ior)
{
	if (same_hemisphere(w_o, w_i, n)) {
		return 0.f;
	}
	bool entering = dot(w_o, n) > 0.f;
	float eta_o = entering ? 1.f : ior;
	float eta_i = entering ? ior : 1.f;

	if (length(w_o + w_i) < EPSILON) return 0;

	vec3 w_h = normalize(w_o + w_i * eta_i / eta_o);

	float cos_theta_h = abs(dot(n, w_h));
	float i_dot_h = dot(w_i, w_h);
	float o_dot_h = dot(w_o, w_h);
	float d = gtr_2(cos_theta_h, alpha); // might become infinite...
	float dwh_dwi = o_dot_h * pow2(eta_o) / pow2(eta_o * o_dot_h + eta_i * i_dot_h);
	
	float transmission = d * cos_theta_h * abs(dwh_dwi);

	return transmission;
}

float gtr_2_aniso_pdf(const in vec3 w_o, const in vec3 w_i, const in vec3 n,
	const in vec3 v_x, const in vec3 v_y, const vec2 alpha)
{
	if (!same_hemisphere(w_o, w_i, n)) {
		return 0.f;
	}
	vec3 w_h = normalize(w_i + w_o);
	float cos_theta_h = dot(n, w_h);
	float d = gtr_2_aniso(cos_theta_h, abs(dot(w_h, v_x)), abs(dot(w_h, v_y)), alpha);
	return d * cos_theta_h / (4.f * dot(w_o, w_h));
}

float lambertian_diffuse(const in MaterialStruct mat, const in vec3 n,
	const in vec3 w_o, const in vec3 w_i, out vec3 diffuse_color)
{
	diffuse_color = mat.base_color.rgb;
	return 1.0f/M_PI;
}

float disney_diffuse(const in MaterialStruct mat, const in vec3 n,
	const in vec3 w_o, const in vec3 w_i, out vec3 diffuse_color)
{
	vec3 w_h = normalize(w_i + w_o);
	float n_dot_o = abs(dot(w_o, n));
	float n_dot_i = abs(dot(w_i, n));
	float i_dot_h = dot(w_i, w_h);
	float fd90 = 0.5f + 2.f * mat.roughness * i_dot_h * i_dot_h;
	float fi = schlick_weight(n_dot_i);
	float fo = schlick_weight(n_dot_o);
	diffuse_color = mat.base_color.rgb;
	return M_1_PI * mix(1.f, fd90, fi) * mix(1.f, fd90, fo);
}

vec3 disney_microfacet_isotropic(const in MaterialStruct mat, const in vec3 n,
	const in vec3 w_o, const in vec3 w_i, out vec3 specular_color)
{
	vec3 w_h = normalize(w_i + w_o);
	float lum = luminance(mat.base_color.rgb);
	vec3 tint = lum > 0.f ? mat.base_color.rgb / lum : vec3(1, 1, 1);
	vec3 spec = mix(mat.specular * 0.08 * mix(vec3(1, 1, 1), tint, mat.specular_tint), mat.base_color.rgb, mat.metallic);
	specular_color = spec;

	vec3 f = mix(spec, vec3(1, 1, 1), schlick_weight(dot(w_i, w_h)));
	float alpha = max(ALPHA_MINIMUM, mat.roughness * mat.roughness);
	float d = gtr_2(dot(n, w_h), alpha);
	float g = smith_shadowing_ggx(dot(n, w_i), alpha) * smith_shadowing_ggx(dot(n, w_o), alpha);
	return f * d * g;
}

float disney_microfacet_transmission_isotropic(const in MaterialStruct mat, const in vec3 n,
	const in vec3 w_o, const in vec3 w_i, out vec3 transmission_color)
{
	float o_dot_n = dot(w_o, n);
	float i_dot_n = dot(w_i, n);
	// These dot products will result in fireflies if let be too small.
	if (abs(o_dot_n) < EPSILON || abs(i_dot_n) < EPSILON) {
		return 0.f;
	}
	if (length(w_o + w_i) < EPSILON) return 0.f;

	bool entering = o_dot_n > 0.f;
	float eta_o = entering ? 1.f : mat.ior;
	float eta_i = entering ? mat.ior : 1.f;
	vec3 w_h = normalize(w_o + w_i * eta_i / eta_o);
	
	
	float alpha = max(ALPHA_MINIMUM, mat.roughness * mat.roughness);
	float d = gtr_2( abs(dot(n, w_h)), alpha); // Seems to be near infinite when n == w_h

	float f = fresnel_dielectric(abs(dot(w_i, n)), eta_i, eta_o); // I think eta_i and eta_o were flipped here. I flipped them back.
	float g = smith_shadowing_ggx(abs(dot(n, w_i)), alpha) * smith_shadowing_ggx(abs(dot(n, w_o)), alpha);

	float i_dot_h = dot(w_i, w_h);
	float o_dot_h = dot(w_o, w_h);

	float c = abs(o_dot_h) / abs(dot(w_o, n))
			* abs(i_dot_h) / abs(dot(w_i, n))
			* pow2(eta_o) / pow2(eta_o * o_dot_h + eta_i * i_dot_h);

	float result = c * (1.f - f) * g * d;
	transmission_color = mat.base_color.rgb;
	return result;
}

vec3 disney_microfacet_anisotropic(const in MaterialStruct mat, const in vec3 n,
	const in vec3 w_o, const in vec3 w_i, const in vec3 v_x, const in vec3 v_y,
	out vec3 specular_color)
{
	vec3 w_h = normalize(w_i + w_o);
	float lum = luminance(mat.base_color.rgb);
	vec3 tint = lum > 0.f ? mat.base_color.rgb / lum : vec3(1, 1, 1);
	vec3 spec = mix(mat.specular * 0.08 * mix(vec3(1, 1, 1), tint, mat.specular_tint), mat.base_color.rgb, mat.metallic);
	specular_color = spec;

	float aspect = sqrt(1.f - mat.anisotropic * 0.9f);
	float a = mat.roughness * mat.roughness;
	vec2 alpha = vec2(max(ALPHA_MINIMUM, a / aspect), max(ALPHA_MINIMUM, a * aspect));
	float d = gtr_2_aniso(dot(n, w_h), abs(dot(w_h, v_x)), abs(dot(w_h, v_y)), alpha);
	vec3 f = mix(spec, vec3(1, 1, 1), schlick_weight(dot(w_i, w_h)));
	float g = smith_shadowing_ggx_aniso(dot(n, w_i), abs(dot(w_i, v_x)), abs(dot(w_i, v_y)), alpha)
		* smith_shadowing_ggx_aniso(dot(n, w_o), abs(dot(w_o, v_x)), abs(dot(w_o, v_y)), alpha);
	return f * d * g;
}

float disney_clear_coat(const in MaterialStruct mat, const in vec3 n,
	const in vec3 w_o, const in vec3 w_i, out vec3 clear_coat_color)
{
	vec3 w_h = normalize(w_i + w_o);
	// float alpha = mix(0.1f, CLEARCOAT_ALPHA_MINIMUM, (1.0f - sqrt(mat.clearcoat_roughness)));
	float alpha = max(ALPHA_MINIMUM, mat.clearcoat_roughness * mat.clearcoat_roughness);	
	float d = gtr_1(dot(n, w_h), alpha);
	float f = mix(0.04f, 1.f, schlick_weight(dot(w_i, n)));
	float g = smith_shadowing_ggx(dot(n, w_i), 0.25f) * smith_shadowing_ggx(dot(n, w_o), 0.25f);
	clear_coat_color = vec3(1.0);
	return 0.25 * mat.clearcoat * d * f * g;
}

vec3 disney_sheen(const in MaterialStruct mat, const in vec3 n,
	const in vec3 w_o, const in vec3 w_i, out vec3 sheen_color)
{
	vec3 w_h = normalize(w_i + w_o);
	float lum = luminance(mat.base_color.rgb);
	vec3 tint = lum > 0.f ? mat.base_color.rgb / lum : vec3(1, 1, 1);
	sheen_color = mix(vec3(1, 1, 1), tint, mat.sheen_tint);
	float f = schlick_weight(dot(w_i, n));
	return f * mat.sheen * sheen_color;
}

void disney_bsdf(const in MaterialStruct mat, bool backface, const in vec3 w_n,
	const in vec3 w_o, const in vec3 w_i, const in vec3 w_x, const in vec3 w_y,
	out vec3 bsdf, out vec3 dbsdf, out vec3 sbsdf)
{
	bsdf = vec3(0.);
	dbsdf = vec3(0.);
	sbsdf = vec3(0.);

	vec3 w_n_f = (backface) ? -w_n : w_n;
	vec3 w_x_f = (backface) ? -w_x : w_x;
	vec3 w_y_f = (backface) ? -w_y : w_y;

	float n_comp = 3.f;

	float spec_trans = 0;
	float transmission = 0;
	vec3 transmission_color = vec3(0.0);
	if (mat.transmission > 0.f) {
		if (!same_hemisphere(w_o, w_i, w_n)) 
		{
			n_comp = 4.f;
			transmission = mat.transmission;
			spec_trans = disney_microfacet_transmission_isotropic(mat, w_n, w_o, w_i, transmission_color);
		} 
	}

	float coat = 0.0, diffuse = 0.0;
	vec3 sheen = vec3(0.0);
	vec3 gloss = vec3(0.0);
	vec3 coat_color = vec3(0.0), sheen_color = vec3(0.0), diffuse_color = vec3(0.0), gloss_color = vec3(0.0);

	if (same_hemisphere(w_o, w_i, w_n_f)) 
	{
		coat = disney_clear_coat(mat, w_n_f, w_o, w_i, coat_color);
		sheen = disney_sheen(mat, w_n_f, w_o, w_i, sheen_color);
		diffuse = disney_diffuse(mat, w_n_f, w_o, w_i, diffuse_color);
		// diffuse = lambertian_diffuse(mat, w_n_f, w_o, w_i, diffuse_color);
		// if (mat.anisotropic == 0.f) {
		gloss = disney_microfacet_isotropic(mat, w_n_f, w_o, w_i, gloss_color);
		// } else {
			// gloss = disney_microfacet_anisotropic(mat, w_n_f, w_o, w_i, w_x_f, w_y_f, gloss_color);
		// }
	}

	/* Combined bsdf */
	bsdf += diffuse_color * diffuse * (1.f - mat.metallic) * (1.f - mat.transmission);
	bsdf += sheen;
	bsdf += gloss;
	bsdf += vec3(coat);
	bsdf += transmission_color * spec_trans * (1.f - mat.metallic) * mat.transmission;
	bsdf *= abs(dot(w_i, w_n));

	/* Just diffuse piece */
	dbsdf += diffuse_color * diffuse * (1.f - mat.metallic) * (1.f - mat.transmission);
	dbsdf *= abs(dot(w_i, w_n));
	 
	/* Just specular piece */
	sbsdf += sheen;
	sbsdf += gloss; 
	sbsdf += vec3(coat); 
	sbsdf += transmission_color * spec_trans * (1.f - mat.metallic) * mat.transmission;
	sbsdf *= abs(dot(w_i, w_n)); 
}

void disney_pdf(const in MaterialStruct mat, bool backface, 
	const in vec3 w_n, const in vec3 w_o, const in vec3 w_i, 
	const in vec3 w_x, const in vec3 w_y, 
	out float pdf, out float dpdf, out float spdf)
{
	vec3 w_n_f = (backface) ? -w_n : w_n;
	vec3 w_x_f = (backface) ? -w_x : w_x;
	vec3 w_y_f = (backface) ? -w_y : w_y;

	float alpha = max(ALPHA_MINIMUM, mat.roughness * mat.roughness);
	float aspect = sqrt(1.f - mat.anisotropic * 0.9f);
	vec2 alpha_aniso = vec2(max(ALPHA_MINIMUM, alpha / aspect), max(ALPHA_MINIMUM, alpha * aspect));

	// float clearcoat_alpha = mix(0.1f, CLEARCOAT_ALPHA_MINIMUM, (1.0f - sqrt(mat.clearcoat_roughness)));
	float clearcoat_alpha = max(ALPHA_MINIMUM, mat.clearcoat_roughness * mat.clearcoat_roughness);

	float diffuse = lambertian_pdf(w_i, w_n_f);
	float clear_coat = gtr_1_pdf(w_o, w_i, w_n_f, clearcoat_alpha);

	float n_comp = 3.f;
	float microfacet;
	float microfacet_transmission = 0.f;
	if (mat.anisotropic == 0.f) {
		microfacet = gtr_2_pdf(w_o, w_i, w_n_f, alpha);
	} else {
		microfacet = gtr_2_aniso_pdf(w_o, w_i, w_n_f, w_x_f, w_y_f, alpha_aniso);
	}
	if (mat.transmission > 0.f) {
		if (!same_hemisphere(w_o, w_i, w_n)) {
			n_comp = 4.f;
			microfacet_transmission = gtr_2_transmission_pdf(w_o, w_i, w_n, alpha, mat.ior);
		}
	}

	pdf = ((diffuse + microfacet + microfacet_transmission + clear_coat) / n_comp);
	// dpdf = ((diffuse + microfacet + microfacet_transmission + clear_coat) / n_comp);
	// spdf = ((diffuse + microfacet + microfacet_transmission + clear_coat) / n_comp);
	dpdf = diffuse;
	spdf = (microfacet + microfacet_transmission + clear_coat) / (n_comp - 1);
}

/* Sample a component of the Disney BRDF, returns the sampled BRDF color,
 * ray reflection direction (w_i) and sample PDF. */
vec3 sample_disney_bsdf (
	ivec2 pixel_seed, int frame_seed, const in MaterialStruct mat, 
	bool backface, const in vec3 w_n,
	const in vec3 w_o, const in vec3 w_x, const in vec3 w_y,
	// bool force_diffuse,
	out vec3 w_i, 
	out float pdf, out float dpdf, out float spdf,
	out vec3 bsdf, out vec3 dbsdf, out vec3 sbsdf)
{
	bsdf = dbsdf = sbsdf = vec3(0);
	vec3 w_n_f = (backface) ? -w_n : w_n;
	vec3 w_x_f = (backface) ? -w_x : w_x;
	vec3 w_y_f = (backface) ? -w_y : w_y;

	bool sample_diffuse = (random() < (mat.roughness * (1.f - mat.metallic) * (1.f - mat.transmission))); 

	int component = 0;
	if (mat.transmission == 0.f) {
		component = int(clamp(random() * 3.f, 0, 2));
	} else {
		component = int(clamp(random() * 4.f, 0, 3));
	}

	// component = 0;

	// if (component == 1) component = 2; // clear coat is breaking things...
	
	vec2 samples = vec2(random(), random());
	if (sample_diffuse || (component == 0)) {
		// Sample diffuse component
		w_i = sample_lambertian_dir(w_n_f, w_x_f, w_y_f, samples);
	} else if (component == 1) {
		// Sample clear coat component
		// float alpha = mix(0.1f, CLEARCOAT_ALPHA_MINIMUM, (1.0f - sqrt(mat.clearcoat_roughness)));
		float alpha = max(ALPHA_MINIMUM, mat.clearcoat_roughness * mat.clearcoat_roughness);
		vec3 w_h = sample_gtr_1_h(w_n_f, w_x_f, w_y_f, alpha, samples);
		w_i = reflect(-w_o, w_h);

		// Invalid reflection, terminate ray
		if (!same_hemisphere(w_o, w_i, w_n_f)) {
			pdf = dpdf = spdf = 0.f;
			w_i = vec3(0.f);
			return bsdf;
		}
	} else if (component == 2) {
		vec3 w_h;
		float alpha = max(ALPHA_MINIMUM, mat.roughness * mat.roughness);
		if (mat.anisotropic == 0.f) {
			w_h = sample_gtr_2_h(w_n_f, w_x_f, w_y_f, alpha, samples);
		} else {
			float aspect = sqrt(1.f - mat.anisotropic * 0.9f);
			vec2 alpha_aniso = vec2(max(ALPHA_MINIMUM, alpha / aspect), max(ALPHA_MINIMUM, alpha * aspect));
			w_h = sample_gtr_2_aniso_h(w_n_f, w_x_f, w_y_f, alpha_aniso, samples);
		}
		w_i = reflect(-w_o, w_h);

		// Invalid reflection, terminate ray
		if (!same_hemisphere(w_o, w_i, w_n_f)) {
			pdf = dpdf = spdf = 0.f;
			w_i = vec3(0.f);
			return bsdf;
		}
	} else {
		// Sample microfacet transmission component
		float alpha = max(ALPHA_MINIMUM, mat.roughness * mat.roughness);
		vec3 w_h = sample_gtr_2_h(w_n, w_x, w_y, alpha, samples);
		if (dot(w_o, w_h) < 0.f) {
			w_h = -w_h;
		}
		bool entering = dot(w_o, w_n) > 0.f;
		w_i = refract(-w_o, w_h, entering ? 1.f / mat.ior : mat.ior);

		// Total internal reflection. 
		if (all(equal(w_i, vec3(0.f)))) {
			pdf = dpdf = spdf = 0.f;
			w_i = vec3(0.f);
			return bsdf;
		}
	}
	disney_pdf(mat, backface, w_n, w_o, w_i, w_x, w_y, pdf, dpdf, spdf);
	disney_bsdf(mat, backface, w_n, w_o, w_i, w_x, w_y, bsdf, dbsdf, sbsdf);
}

#endif

