#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D diffuseTexture;
uniform vec3 viewPos;

struct DirLight {
	vec3 direction;
	vec3 color;
	float intensity;
};
uniform DirLight dirLight;

struct SpotLight {
	vec3 position;
	vec3 direction;

	float innerCutoff;
	float outerCutoff;

	vec3 color;

	float constant;
	float linear;
	float quadratic;
};
uniform SpotLight spotLight;

void main()
{
	vec3 N = normalize(Normal);
	vec3 V = normalize(viewPos - FragPos);
	vec3 albedo = texture(diffuseTexture, TexCoord).rgb;

	vec3 Ld = normalize(-dirLight.direction);

	vec3 ambientD = 0.1 * dirLight.color * dirLight.intensity;

	float diffD = max(dot(N, Ld), 0.0);
	vec3 diffuseD = diffD * dirLight.color * dirLight.intensity;

	vec3 reflectD = reflect(-Ld, N);
	float specD = pow(max(dot(V, reflectD), 0.0), 32.0);
	vec3 specularD = specD * dirLight.color * dirLight.intensity;

	vec3 result = ambientD + diffuseD + specularD;

	vec3 Ls = normalize(spotLight.position - FragPos);

	float theta = dot(Ls, normalize(-spotLight.direction));
	float epsilon = spotLight.innerCutoff - spotLight.outerCutoff;
	float intensity = clamp((theta - spotLight.outerCutoff) / epsilon, 0.0, 1.0);

	float distance = length(spotLight.position - FragPos);
	float attenuation = 1.0 / (
		spotLight.constant +
		spotLight.linear * distance +
		spotLight.quadratic * distance * distance
	);

	vec3 ambientS = 0.05 * spotLight.color;

	float diffS = max(dot(N, Ls), 0.0);
	vec3 diffuseS = diffS * spotLight.color;

	vec3 reflectS = reflect(-Ls, N);
	float specS = pow(max(dot(V, reflectS), 0.0), 32.0);
	vec3 specularS = specS * spotLight.color;

	result += (ambientS + diffuseS + specularS) * intensity * attenuation;

	FragColor = vec4(result * albedo, 1.0);
}
