#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 mvp;
uniform mat4 model;
uniform mat3 normalMatrix;

uniform float morphFactor;
uniform vec3 modelCenter;
uniform float sphereRadius;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
	vec3 dir = normalize(aPosition - modelCenter);
	vec3 spherePos = modelCenter + dir * sphereRadius;

	vec3 morphedPos = mix(aPosition, spherePos, morphFactor);

	FragPos = vec3(model * vec4(morphedPos, 1.0));
	Normal  = normalize(normalMatrix * normalize(mix(aNormal, dir, morphFactor)));

	TexCoord = aTexCoord;
	gl_Position = mvp * vec4(morphedPos, 1.0);
}
