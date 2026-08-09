#version 450

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform UniformBufferObject {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
} ubo;

layout (location = 0) in vec3 N;
layout (location = 1) in vec3 L;
layout (location = 2) in vec3 V;
layout (location = 3) in float dist;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, ubo.albedo, ubo.metallic);

    // light radiance
    vec3 Lo = vec3(0.0);

    // calculate light radiance
    vec3 H = normalize(V + L);
    float attenuation = 1.0 / (dist * dist);
    vec3 radiance     = lightColor * attenuation;        
    
    // cook-torrance brdf
    float NDF = DistributionGGX(N, H, ubo.roughness);        
    float G   = GeometrySmith(N,V, L, ubo.roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - ubo.metallic;	  
    
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;  
        
    // add to outgoing radiance Lo
    float NdotL = max(dot(N, L), 0.0);                
    Lo += (kD * ubo.albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * ubo.albedo * ubo.ao;
    vec3 color = ambient + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  
   
    outColor = vec4(color, 1.0);

    // // ambient
    // vec3 ambientColor = ubo.albedo * 0.05;
    //
    // // specular
    // vec3 reflected = reflect(-L, normal);
    // float specular = dot(reflected, V);
    // specular = pow(max(specular, 0.0), 25);
    // vec3 specularColor = ubo.albedo * lightColor * specular;
    //
    // // diffuse
    // float diffuse = dot(L, normal);
    // diffuse = max(diffuse, 0.0);
    // vec3 diffuseColor = ubo.albedo * lightColor * diffuse;
    //
    // outColor = vec4(specular + diffuseColor + ambientColor, 1.0f);
}
