#version 450

uniform sampler2D tex;

in vec3 position;
in vec2 texCoord;
in vec3 normal;
out vec4 FragColor;

void main() {
	FragColor = texture(tex, texCoord);
}
