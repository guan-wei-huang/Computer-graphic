#version 330 core

struct Material {
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float shininess;
};

struct Directional{
	vec3 position;
	vec3 direction;
	vec3 diffuse;
	vec3 ambient;
};

struct Point{
	vec3 position;
	float constant;
	float linear;
	float quadratic;
	vec3 diffuse;
	vec3 ambient;
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
};

out vec4 FragColor;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec3 fragpos;

uniform Material material;
uniform Directional directional;
uniform Point point;
uniform Spot spot;
uniform int lightmode;
uniform int vertex_pixel;

uniform mat4 mvp;
uniform mat4 mv;
uniform mat4 view_matrix;

vec3 CalcuDirecLight(vec3 N, vec3 V1);
vec3 CalcuPointLight(vec3 N, vec3 V1);
vec3 CalcuSpotLight(vec3 N, vec3 V1);

void main() {
	// [TODO]
	vec3 N = normalize(vertex_normal);
	vec3 V = fragpos;

	vec3 color = vec3(0, 0, 0);
	if (lightmode == 0) {
		color = CalcuDirecLight(N, V);
	} else if (lightmode == 1) {
		color = CalcuPointLight(N, V);
	} else {
		color = CalcuSpotLight(N, V);
	}	

	if (vertex_pixel == 0) {
		FragColor = vec4(vertex_color, 1.0f);
	} else {
		FragColor = vec4(color, 1.0f);
	}
}

vec3 CalcuDirecLight(vec3 N, vec3 V) {
	vec3 L = normalize(directional.position - V);
	vec3 E = normalize(-V);
	vec3 H = reflect(-L, N);	
	vec3 ambient = material.ambient * directional.ambient;
	vec3 diffuse = material.diffuse * directional.diffuse * max(dot(N, L), 0.0);
	vec3 specular = material.specular * pow(max(dot(H, E), 0.0), material.shininess);

	return ambient + diffuse + specular;
}

vec3 CalcuPointLight(vec3 N, vec3 V1) {
	vec4 lightInView = view_matrix * vec4(point.position, 1.0);
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
	vec4 lightInView = view_matrix * vec4(spot.position, 1.0);
	vec3 S = normalize(lightInView.xyz);
	vec3 V = normalize(-V1);
	vec3 H = normalize(S + V);
	vec3 lightVec = lightInView.xyz - V1;
	vec3 L = normalize(lightVec);
	float lightDis = length(spot.position - V1);
	float attenuation = 1.0 / (spot.constant + spot.linear * lightDis + spot.quadratic * lightDis * lightDis);

	float spotCos = dot(-L, normalize((transpose(inverse(view_matrix)) * vec4(spot.direction, 1.0)).xyz));
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