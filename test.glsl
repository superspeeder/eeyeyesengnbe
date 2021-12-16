#type fragment
#version 430 core

uniform vec4 uColor;

out vec4 outColor;

float near = 0.1; 
float far  = 100.0; 
  
float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));	
}


void main() {
//	outColor = uColor;
	outColor = vec4((1 - LinearizeDepth(gl_FragCoord.z) / 4) * uColor.rgb, 1.0);
}

