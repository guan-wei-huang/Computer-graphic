#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;

struct Material {
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float shininess;
	float diffuseTexture;
};

struct Directional{
	vec3 position;
	vec3 direction;
	vec3 diffuse;
	vec3 ambient;
	//vec3 color;
};

struct Point{
	vec3 position;
	float constant;
	float linear;
	float quadratic;
	vec3 diffuse;
	vec3 ambient;
	//vec3 color;
};

struct Spot{
	vec3 position;
	vec3 direction;
	float exponent;
	float cutoff;
	float constant;
	float linear;
	float quadratic;
	vec3 diffuse;
	vec3 ambient;
	//vec3 color;
};

out vec2 texCoord;
out vec3 vertex_color;
out vec3 vertex_normal;
out vec3 fragpos;

uniform mat4 um4p;
uniform mat4 um4v;
uniform mat4 um4m;

uniform Material material;
uniform Directional directional;
uniform Point point;
uniform Spot spot;
uniform int lightmode;
uniform int per_vertex_or_per_pixel;


vec3 CalcuDirecLight(vec3 N, vec3 V1);
vec3 CalcuPointLight(vec3 N, vec3 V1);
vec3 CalcuSpotLight(vec3 N, vec3 V1);

void main() 
{
	// [TODO]
	gl_Position = um4p * um4v * um4m * vec4(aPos, 1.0);

	vec4 Fragpos = um4v * um4m * vec4(aPos.x, aPos.y, aPos.z, 1.0);
	fragpos = Fragpos.xyz;
	vertex_normal = mat3(transpose(inverse(um4v * um4m))) * aNormal;
	texCoord = aTexCoord;

	vec3 V = fragpos;
	vec3 N = normalize(vertex_normal);
	if (lightmode == 0) {
		vertex_color = CalcuDirecLight(N, V);
	} else if (lightmode == 1) {
		vertex_color = CalcuPointLight(N, V);
	} else {
		vertex_color = CalcuSpotLight(N, V);
	}
}


vec3 CalcuDirecLight(vec3 N, vec3 V) {
	vec3 L = normalize(directional.position - V);
	vec3 E = normalize(-V);
	vec3 H = reflect(-L, N);	
	vec3 ambient = directional.ambient * material.ambient;
	vec3 diffuse = directional.diffuse * material.diffuse * max(dot(N, L), 0);
	vec3 specular = material.specular * pow(max(dot(H, E), 0.0), material.shininess);

	return ambient + diffuse + specular;;
}

vec3 CalcuPointLight(vec3 N, vec3 V1) {
	vec4 lightInView = um4v * vec4(point.position, 1.0);
	vec3 S = normalize(lightInView.xyz);
	vec3 V = normalize(-V1);
	vec3 H = normalize(S + V);
	vec3 lightVec = lightInView.xyz - V1;
	vec3 L = normalize(lightVec);
	float lightDis = length(point.position - V1);
	float attenuation = 1.0 / (point.constant + point.linear * lightDis + point.quadratic * lightDis * lightDis);
	
	vec3 ambient = material.ambient * point.ambient;
	vec3 diffuse = material.diffuse * point.diffuse * max(dot(N, L), 0.0);
	vec3 specular = material.specular * pow(max(dot(N, H), 0.0), material.shininess);

	return ambient + (diffuse + specular) * attenuation;
}

vec3 CalcuSpotLight(vec3 N, vec3 V1) {
	vec4 lightInView = um4v * vec4(spot.position, 1.0);
	vec3 S = normalize(lightInView.xyz);
	vec3 V = normalize(-V1);
	vec3 H = normalize(S + V);
	vec3 lightVec = lightInView.xyz - V1;
	vec3 L = normalize(lightVec);
	float lightDis = length(spot.position - V1);
	float attenuation = 1.0 / (spot.constant + spot.linear * lightDis + spot.quadratic * lightDis * lightDis);

	float spotCos = dot(-L, normalize((transpose(inverse(um4v)) * vec4(spot.direction, 1.0)).xyz));
	float exponent = pow(max(spotCos, 0.0), spot.exponent);
	if (spot.cutoff <= degrees(acos(spotCos))) {
		return spot.ambient * material.ambient;
	} else {
		vec3 ambient = spot.ambient * material.ambient;
		vec3 diffuse = (dot(L, N) * spot.diffuse * material.diffuse);
		vec3 specular = pow(max(dot(H, N), 0.0), material.shininess) * material.specular;
		return  ambient + attenuation * exponent * (diffuse + specular);
	}
}
