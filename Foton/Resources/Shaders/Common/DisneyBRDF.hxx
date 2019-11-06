#include "Foton/Resources/Shaders/Common/Utilities.hxx"

// Disney brdf's taken from here:: https://github.com/wdas/brdf/blob/master/src/brdfs/disney.brdf

// required helper functions


float distanceSq(vec3 v1, vec3 v2) {
    vec3 d = v1 - v2;
    return dot(d, d);
}

void createBasis(vec3 normal, out vec3 tangent, out vec3 binormal){
    if (abs(normal.x) > abs(normal.y)) {
        tangent = normalize(vec3(0., normal.z, -normal.y));
    }
    else {
        tangent = normalize(vec3(-normal.z, 0., normal.x));
    }
    
    binormal = cross(normal, tangent);
}

bool sameHemiSphere(const in vec3 wo, const in vec3 wi, const in vec3 normal) {
    return dot(wo, normal) * dot(wi, normal) > 0.0;
}

vec3 sphericalDirection(float sinTheta, float cosTheta, float sinPhi, float cosPhi) {
    return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

vec3 uniformSampleCone(vec2 u12, float cosThetaMax, vec3 xbasis, vec3 ybasis, vec3 zbasis) {
    float cosTheta = (1. - u12.x) + u12.x * cosThetaMax;
    float sinTheta = sqrt(1. - cosTheta * cosTheta);
    float phi = u12.y * TWO_PI;
    vec3 samplev = sphericalDirection(sinTheta, cosTheta, sin(phi), cos(phi));
    return samplev.x * xbasis + samplev.y * ybasis + samplev.z * zbasis;
}

vec2 concentricSampleDisk(const in vec2 u) {
    vec2 uOffset = 2. * u - vec2(1., 1.);

    if (uOffset.x == 0. && uOffset.y == 0.) return vec2(0., 0.);

    float theta, r;
    if (abs(uOffset.x) > abs(uOffset.y)) {
        r = uOffset.x;
        theta = PI/4. * (uOffset.y / uOffset.x);
    } else {
        r = uOffset.y;
        theta = PI/2. - PI/4. * (uOffset.x / uOffset.y);
    }
    return r * vec2(cos(theta), sin(theta));
}
vec3 cosineSampleHemisphere(const in vec2 u) {
    vec2 d = concentricSampleDisk(u);
    float z = sqrt(max(EPSILON, 1. - d.x * d.x - d.y * d.y));
    return vec3(d.x, d.y, z);
}

vec3 uniformSampleHemisphere(const in vec2 u) {
    float z = u[0];
    float r = sqrt(max(EPSILON, 1. - z * z));
    float phi = 2. * PI * u[1];
    return vec3(r * cos(phi), r * sin(phi), z);
}

float powerHeuristic(float nf, float fPdf, float ng, float gPdf){
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f*f)/(f*f + g*g);
}

float schlickWeight(float cosTheta) {
    float m = clamp(1. - cosTheta, 0., 1.);
    return (m * m) * (m * m) * m;
}

float GTR1(float NdotH, float a) {
    if (a >= 1.) return 1./PI;
    float a2 = a*a;
    float t = 1. + (a2-1.)*NdotH*NdotH;
    return (a2-1.) / (PI*log(a2)*t);
}

float GTR2(float NdotH, float a) {
    float a2 = a*a;
    float t = 1. + (a2-1.)*NdotH*NdotH;
    return a2 / (PI * t*t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay) {
    return 1. / (PI * ax*ay * pow2( pow2(HdotX/ax) + pow2(HdotY/ay) + NdotH*NdotH ));
}

float smithG_GGX(float NdotV, float alphaG) {
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1. / (abs(NdotV) + max(sqrt(a + b - a*b), EPSILON));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay) {
    return 1. / (NdotV + sqrt( pow2(VdotX*ax) + pow2(VdotY*ay) + pow2(NdotV) ));
}

float pdfLambertianReflection(const in vec3 wi, const in vec3 wo, const in vec3 normal) {
    return sameHemiSphere(wo, wi, normal) ? abs(dot(normal, wi))/PI : 0.;
}

float pdfMicrofacet(const in vec3 wi, const in vec3 wo, const in SurfaceInteraction interaction, const in MaterialStruct material) {
    if (!sameHemiSphere(wo, wi, interaction.w_n.xyz)) return 0.;
    vec3 wh = normalize(wo + wi);
    
    float NdotH = dot(interaction.w_n.xyz, wh);
    float alpha2 = material.roughness * material.roughness;
    alpha2 *= alpha2;
    
    float cos2Theta = NdotH * NdotH;
    float denom = cos2Theta * ( alpha2 - 1.) + 1.;
    if( denom == 0. ) return 0.;
    float pdfDistribution = alpha2 * NdotH /(PI * denom * denom);
    return pdfDistribution/(4. * dot(wo, wh));
}

float pdfMicrofacetAniso(const in vec3 wi, const in vec3 wo, const in SurfaceInteraction interaction, const in MaterialStruct material) {
    if (!sameHemiSphere(wo, wi, interaction.w_n.xyz)) return 0.;
    vec3 wh = normalize(wo + wi);
    
    float aspect = sqrt(1.-material.anisotropic*.9);
    float alphax = max(.001, pow2(material.roughness)/aspect);
    float alphay = max(.001, pow2(material.roughness)*aspect);
    
    float alphax2 = alphax * alphax;
    float alphay2 = alphax * alphay;

    float hDotX = dot(wh, interaction.w_t.xyz);
    float hDotY = dot(wh, interaction.w_b.xyz);
    float NdotH = dot(interaction.w_n.xyz, wh);
    
    float denom = hDotX * hDotX/alphax2 + hDotY * hDotY/alphay2 + NdotH * NdotH;
    if( denom == 0. ) return 0.;
    float pdfDistribution = NdotH /(PI * alphax * alphay * denom * denom);
    return pdfDistribution/(4. * dot(wo, wh));
}

float pdfClearCoat(const in vec3 wi, const in vec3 wo, const in SurfaceInteraction interaction, const in MaterialStruct material) {
    if (!sameHemiSphere(wo, wi, interaction.w_n.xyz)) return 0.;

    vec3 wh = wi + wo;
    wh = normalize(wh);
	
    float NdotH = abs(dot(wh, interaction.w_n.xyz));
    float Dr = GTR1(NdotH, mix(.1,.001,1.0 - material.clearcoat_roughness));
    return Dr * NdotH/ (4. * dot(wo, wh));
}

vec3 disneyDiffuse(const in float NdotL, const in float NdotV, const in float LdotH, const in MaterialStruct material) {
    float FL = schlickWeight(NdotL), FV = schlickWeight(NdotV);
    
    float Fd90 = 0.5 + 2. * LdotH*LdotH * material.roughness;
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);
    
    return (1./PI) * Fd * material.base_color.rgb;
}

#define cosineSample \
	vec3 wiLocal = cosineSampleHemisphere(u); \
	vec3 tangent = vec3(0.), binormal = vec3(0.);\
	createBasis(normal, tangent, binormal);\
	wi = wiLocal.x * tangent + wiLocal.y * binormal + wiLocal.z * normal;\
    if (dot(wo, normal) < 0.) wi.z *= -1.;\

void disneyDiffuseSample(out vec3 wi, const in vec3 wo, out float pdf, const in vec2 u, const in vec3 normal, const in MaterialStruct material) {
    cosineSample
}

vec3 disneySubsurface(const in float NdotL, const in float NdotV, const in float LdotH, const in MaterialStruct material) {
    
    float FL = schlickWeight(NdotL), FV = schlickWeight(NdotV);
    float Fss90 = LdotH*LdotH*material.roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1. / (NdotL + NdotV) - .5) + .5);
    
    return (1./PI) * ss * material.base_color.rgb;
}

vec3 disneyMicrofacetIsotropic(float NdotL, float NdotV, float NdotH, float LdotH, const in MaterialStruct material) {
    
    float Cdlum = .3*material.base_color.r + .6*material.base_color.g + .1*material.base_color.b; // luminance approx.

    vec3 Ctint = Cdlum > 0. ? material.base_color.rgb/Cdlum : vec3(1.); // normalize lum. to isolate hue+sat
    vec3 Cspec0 = mix(material.specular *.08 * mix(vec3(1.), Ctint, material.specular_tint), material.base_color.rgb, material.metallic);
    
    float a = max(.001, pow2(material.roughness));
    float Ds = GTR2(NdotH, a);
    float FH = schlickWeight(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1), FH);
    float Gs;
    Gs  = smithG_GGX(NdotL, a);
    Gs *= smithG_GGX(NdotV, a);
    
    return Gs*Fs*Ds;
}

vec3 disneyMicrofacetAnisotropic(float NdotL, float NdotV, float NdotH, float LdotH,
                                 const in vec3 L, const in vec3 V,
                                 const in vec3 H, const in vec3 X, const in vec3 Y,
                                 const in MaterialStruct material) {
    
    float Cdlum = .3*material.base_color.r + .6*material.base_color.g + .1*material.base_color.b;

    vec3 Ctint = Cdlum > 0. ? material.base_color.rgb/Cdlum : vec3(1.);
    vec3 Cspec0 = mix(material.specular *.08 * mix(vec3(1.), Ctint, material.specular_tint), material.base_color.rgb, material.metallic);
    
    float aspect = sqrt(1.-material.anisotropic*.9);
    float ax = max(.001, pow2(material.roughness)/aspect);
    float ay = max(.001, pow2(material.roughness)*aspect);
    float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
    float FH = schlickWeight(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1), FH);
    float Gs;
    Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
    Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);
    
    return Gs*Fs*Ds;
}

void disneyMicrofacetAnisoSample(out vec3 wi, const in vec3 wo, /* const in vec3 X, const in vec3 Y,*/ const in vec2 u, const in SurfaceInteraction interaction, const in MaterialStruct material) {
    float cosTheta = 0., phi = 0.;
    
    float aspect = sqrt(1. - material.anisotropic*.9);
    float alphax = max(.001, pow2(material.roughness)/aspect);
    float alphay = max(.001, pow2(material.roughness)*aspect);
    
    phi = atan(alphay / alphax * tan(2. * PI * u[1] + .5 * PI));
    
    if (u[1] > .5f) phi += PI;
    float sinPhi = sin(phi), cosPhi = cos(phi);
    float alphax2 = alphax * alphax, alphay2 = alphay * alphay;
    float alpha2 = 1. / (cosPhi * cosPhi / alphax2 + sinPhi * sinPhi / alphay2);
    float tanTheta2 = alpha2 * u[0] / (1. - u[0]);
    cosTheta = 1. / sqrt( max(1. + tanTheta2, EPSILON));
    
    float sinTheta = sqrt(max(0., 1. - cosTheta * cosTheta));
    vec3 whLocal = sphericalDirection(sinTheta, cosTheta, sin(phi), cos(phi));
         
    vec3 wh = whLocal.x * interaction.w_t.xyz + whLocal.y * interaction.w_b.xyz + whLocal.z * interaction.w_n.xyz;

    if(!sameHemiSphere(wo, wh, interaction.w_n.xyz)) {
       wh *= -1.;
    }

    wi = reflect(-wo, wh);
}

float disneyClearCoat(float NdotL, float NdotV, float NdotH, float LdotH, const in MaterialStruct material) {
    float gloss = mix(.1,.001,1.0 - material.clearcoat_roughness);
    float Dr = GTR1(abs(NdotH), gloss);
    float FH = schlickWeight(LdotH);
    float Fr = mix(.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);
    return /* clearCoatBoost **/ material.clearcoat * Fr * Gr * Dr;
}

void disneyClearCoatSample(out vec3 wi, const in vec3 wo, const in vec2 u, const in SurfaceInteraction interaction, const in MaterialStruct material) {
	float gloss = mix(0.1, 0.001, 1.0 - material.clearcoat_roughness);
    float alpha2 = gloss * gloss;
    float cosTheta = sqrt(max(EPSILON, (1. - pow(alpha2, 1. - u[0])) / (1. - alpha2)));
    float sinTheta = sqrt(max(EPSILON, 1. - cosTheta * cosTheta));
    float phi = TWO_PI * u[1];
    
    vec3 whLocal = sphericalDirection(sinTheta, cosTheta, sin(phi), cos(phi));
     
    vec3 tangent = vec3(0.), binormal = vec3(0.);
    createBasis(interaction.w_n.xyz, tangent, binormal);
    
    vec3 wh = whLocal.x * tangent + whLocal.y * binormal + whLocal.z * interaction.w_n.xyz;
    
    if(!sameHemiSphere(wo, wh, interaction.w_n.xyz)) {
       wh *= -1.;
    }
            
    wi = reflect(-wo, wh);   
}

vec3 disneySheen(float LdotH, const in MaterialStruct material) {
    float FH = schlickWeight(LdotH);
    float Cdlum = .3*material.base_color.r + .6*material.base_color.g  + .1*material.base_color.b;

    vec3 Ctint = Cdlum > 0. ? material.base_color.rgb/Cdlum : vec3(1.);
    vec3 Csheen = mix(vec3(1.), Ctint, material.sheen_tint);
    vec3 Fsheen = FH * material.sheen * Csheen;
    return FH * material.sheen * Csheen;
}

vec3 bsdfEvaluate(const in vec3 wi, const in vec3 wo, /* const in vec3 X, const in vec3 Y,*/ const in SurfaceInteraction interaction, const in MaterialStruct material) {
    if( !sameHemiSphere(wo, wi, interaction.w_n.xyz) )
        return vec3(0.);
    
	float NdotL = dot(interaction.w_n.xyz, wo);
    float NdotV = dot(interaction.w_n.xyz, wi);
    
    if (NdotL < 0. || NdotV < 0.) return vec3(0.);

    vec3 H = normalize(wo+wi);
    float NdotH = dot(interaction.w_n.xyz,H);
    float LdotH = dot(wo,H);

    //TEMPORARY
    
    vec3 diffuse = disneyDiffuse(NdotL, NdotV, LdotH, material);
    vec3 subSurface = disneySubsurface(NdotL, NdotV, LdotH, material);
    vec3 glossy = disneyMicrofacetAnisotropic(NdotL, NdotV, NdotH, LdotH, wi, wo, H, interaction.w_t.xyz, interaction.w_b.xyz, material);
    // vec3 glossy = disneyMicrofacetIsotropic(NdotL, NdotV, NdotH, LdotH, material);
    float clearCoat = disneyClearCoat(NdotL, NdotV, NdotH, LdotH, material);
    vec3 sheen = disneySheen(LdotH, material);
    
    return ( mix(diffuse, subSurface, material.subsurface) + sheen ) * (1. - material.metallic) + glossy + clearCoat;
}

float bsdfPdf(const in vec3 wi, const in vec3 wo, const in SurfaceInteraction interaction, const in MaterialStruct material) {
    float pdfDiffuse = pdfLambertianReflection(wi, wo, interaction.w_n.xyz);
    float pdfMicrofacet = pdfMicrofacetAniso(wi, wo, interaction, material);
    float pdfClearCoat = pdfClearCoat(wi, wo, interaction, material);;
    return (pdfDiffuse + pdfMicrofacet + pdfClearCoat)/3.;
}

vec3 bsdfSample (float random1, float random2, float random3, out vec3 wi, const in vec3 wo, /*const in vec3 X, const in vec3 Y,*/  out float pdf, out bool is_specular, const in SurfaceInteraction interaction, const in MaterialStruct material) {
    
    vec3 f = vec3(0.);
    pdf = 0.0;
	wi = vec3(0.);
    
    vec2 u = vec2(random1, random2);
    float rnd = random3;
	if( rnd <= 0.3333 ) {
       disneyDiffuseSample(wi, wo, pdf, u, interaction.w_n.xyz, material);
       is_specular = false;
    }
    else if( rnd >= 0.3333 && rnd < 0.6666 ) {
       disneyMicrofacetAnisoSample(wi, wo, u, interaction, material);
       is_specular = true;
    }
    else {
       disneyClearCoatSample(wi, wo, u, interaction, material);
       is_specular = true;
    }
    f = bsdfEvaluate(wi, wo, interaction, material);
    pdf = bsdfPdf(wi, wo, interaction, material);
    if( pdf < EPSILON )
        return vec3(0.);
	return f;
}