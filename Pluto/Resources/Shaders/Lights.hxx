#ifndef LIGHTS_HXX
#define LIGHTS_HXX

// vec3 pointLightSample(const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n, out vec3 w_i, out float lightPdf) {
//     vec3 w_light_position = light_transform.localToWorld[3].xyz;
//     vec3 w_light_dir = normalize(w_light_position - w_p);
    
//     w_i = w_light_dir;    
//     float sinThetaMax2 = 1.0 / distanceSq(w_light_position, w_p);
//     float cosThetaMax = sqrt(max(EPSILON, 1. - sinThetaMax2));
//     if (dot(w_i, w_n) > 0.) {
//         lightPdf = 1. / (TWO_PI * (1. - cosThetaMax));
//     }
    
// 	return light.color.rgb * light.intensity;
// }

// float pointLightPDF( const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n ) { 
//     vec3 w_light_right =    light_transform.localToWorld[0].xyz;
//     vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
//     vec3 w_light_up =       light_transform.localToWorld[2].xyz;
//     vec3 w_light_position = light_transform.localToWorld[3].xyz;
//     vec3 w_light_dir = normalize(w_light_position - w_p);
    
//     float s1 = length(w_light_right);
//     float s2 = length(w_light_forward);
//     float s3 = length(w_light_up);

//     float light_radius = max(max(abs(s1), abs(s2) ), abs(s3) );

//     float sinThetaMax2 = light_radius * light_radius / distanceSq(w_light_position, w_p);
//     float cosThetaMax = sqrt(max(EPSILON, 1. - sinThetaMax2));
//     return 1. / (TWO_PI * (1. - cosThetaMax)); 
// }

vec3 sphereLightSample(const in ivec2 pixel_coords, const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n, out vec3 w_i, out float lightPdf) {
    vec3 w_light_right =    light_transform.localToWorld[0].xyz;
    vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
    vec3 w_light_up =       light_transform.localToWorld[2].xyz;
    vec3 w_light_position = light_transform.localToWorld[3].xyz;
    vec3 w_light_dir = normalize(w_light_position - w_p);
    
    float s1 = length(w_light_right);
    float s2 = length(w_light_forward);
    float s3 = length(w_light_up);

    float light_radius = max(max(abs(s1), abs(s2) ), abs(s3) );
    
    vec2 u = vec2(random(pixel_coords), random(pixel_coords));
    
    vec3 tangent = vec3(0.), binormal = vec3(0.);
    createBasis(w_light_dir, tangent, binormal);
    
    float sinThetaMax2 = light_radius * light_radius / distanceSq(w_light_position, w_p);
    float cosThetaMax = sqrt(max(EPSILON, 1. - sinThetaMax2));
    w_i = uniformSampleCone(u, cosThetaMax, tangent, binormal, w_light_dir);
    
    if (dot(w_i, w_n) > 0.) {
        // lightPdf = 1. / (TWO_PI * (1. - cosThetaMax));
        vec3 to_pt = w_light_position - w_p;
        float dist_sqr = dot(to_pt, to_pt);
        lightPdf = dist_sqr / (TWO_PI * (1. - cosThetaMax)); 
    }
    
	return light.color.rgb * light.intensity;
}

float sphereLightPDF( const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n ) { 
    vec3 w_light_right =    light_transform.localToWorld[0].xyz;
    vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
    vec3 w_light_up =       light_transform.localToWorld[2].xyz;
    vec3 w_light_position = light_transform.localToWorld[3].xyz;
    vec3 w_light_dir = normalize(w_light_position - w_p);
    
    float s1 = length(w_light_right);
    float s2 = length(w_light_forward);
    float s3 = length(w_light_up);

    float light_radius = max(max(abs(s1), abs(s2) ), abs(s3) );

    float sinThetaMax2 = light_radius * light_radius / distanceSq(w_light_position, w_p);
    float cosThetaMax = sqrt(max(EPSILON, 1. - sinThetaMax2));
    vec3 to_pt = w_light_position - w_p;
	float dist_sqr = dot(to_pt, to_pt);
    return dist_sqr / (TWO_PI * (1. - cosThetaMax)); 
}

vec3 rectangleLightSample(const in ivec2 pixel_coords, const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n, out vec3 w_i, out float lightPdf) {
    vec3 w_light_right =    light_transform.localToWorld[0].xyz;
    vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
    vec3 w_light_up =       light_transform.localToWorld[2].xyz;
    vec3 w_light_position = light_transform.localToWorld[3].xyz;
    vec3 w_light_dir = normalize(w_light_position - w_p);

    /* Compute a random point on a unit plane */
    vec4 m_light_position = vec4(random(pixel_coords) * 2.0 - 1.0, random(pixel_coords) * 2.0 - 1.0, 0.0, 1.0);

    /* Transform that point to where the light actually is */
    vec4 w_light_sample_position = light_transform.localToWorld * m_light_position;

    vec3 to_light = w_light_sample_position.xyz - w_p;
    w_i = normalize(to_light);

    float n_dot_w = dot(normalize(w_light_up.xyz), normalize(w_i));

    float width = length(w_light_right);
    float height = length(w_light_forward);


    float surface_area = width * height;
	vec3 to_pt = to_light;//p - dir;
	float dist_sqr = dot(to_pt, to_pt);
	// float n_dot_w = dot(normalize(w_light_up.xyz), wi);
	// if (n_dot_w < EPSILON) {
	// 	lightPdf = 0.f;
	// }
	lightPdf = dist_sqr / ( n_dot_w * surface_area);

	return light.color.rgb * light.intensity;
}

float rectangleLightPDF( const in EntityStruct light_entity, const in LightStruct light, const in TransformStruct light_transform, const in vec3 w_p, const in vec3 w_n ) { 
    vec3 w_light_right =    light_transform.localToWorld[0].xyz;
    vec3 w_light_forward =  light_transform.localToWorld[1].xyz;
    vec3 w_light_up =       light_transform.localToWorld[2].xyz;
    vec3 w_light_position = light_transform.localToWorld[3].xyz;
    vec3 to_light = w_light_position - w_p;
    vec3 w_light_dir = normalize(w_light_position - w_p);
    
    float n_dot_w = dot(normalize(w_light_up.xyz), w_light_dir);

    float width = length(w_light_right);
    float height = length(w_light_forward);

    float surface_area = width * height;
	vec3 to_pt = to_light;//p - dir;
	float dist_sqr = dot(to_pt, to_pt);
	// float n_dot_w = dot(normalize(w_light_up.xyz), wi);
	// if (n_dot_w < EPSILON) {
	// 	lightPdf = 0.f;
	// }
    return dist_sqr / ( n_dot_w * surface_area);
}

#endif