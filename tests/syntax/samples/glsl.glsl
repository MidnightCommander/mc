/* Vertex shader: transform and lighting */

#version 330 core
#define MAX_LIGHTS 8
#ifdef USE_SHADOWS
#ifndef SHADOW_MAP_SIZE
#define SHADOW_MAP_SIZE 1024
#endif
#endif

// Precision qualifiers
precision highp float;
precision mediump int;
precision lowp sampler2D;

// Vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

// Uniforms
uniform mat4 uModelView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform float uTime;

// Outputs to fragment shader
out vec3 vNormal;
out vec2 vTexCoord;
flat out int vInstanceID;

// Varying (deprecated but valid)
varying vec4 vColor;

// Struct definition
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    bool enabled;
};

// Buffer and shared qualifiers
buffer StorageBlock {
    vec4 data[];
};

shared float sharedValues[256];

// Const values
const float PI = 3.14159265;
const int MAX_ITER = 100;

// Type examples: matrices, vectors, samplers
mat2 rotation2D;
mat3x4 mixedMatrix;
ivec4 indices;
bvec3 flags;
dvec2 highPrecCoord;
sampler2D diffuseMap;
samplerCube envMap;

// Main vertex shader function
void main() {
    vec4 worldPos = uModelView * vec4(aPosition, 1.0);

    // Operators
    float a = 1.0 + 2.0 - 3.0 * 4.0 / 5.0;
    int b = 10 % 3;
    bool c = (a > b) && (a != 0.0) || !(a <= b);
    int d = 0xFF & 0x0F | 0xA0 ^ 0x55;
    float e = float(d);
    a += 1.0;
    a -= 0.5;
    a *= 2.0;
    a /= 3.0;
    d++;
    d--;

    // Built-in functions
    float len = length(aPosition);
    vec3 n = normalize(aNormal);
    float dp = dot(n, vec3(0.0, 1.0, 0.0));
    vec3 cp = cross(n, vec3(1.0, 0.0, 0.0));
    float cl = clamp(dp, 0.0, 1.0);
    float mx = max(a, e);
    float mn = min(a, e);
    float sm = smoothstep(0.0, 1.0, cl);
    float mi = mix(mn, mx, sm);
    float sq = sqrt(len);
    float pw = pow(sq, 2.0);
    float sn = sin(uTime);
    float cs = cos(uTime);

    // Texture operations
    vec4 texColor = texture(diffuseMap, aTexCoord);
    ivec2 texSize = textureSize(diffuseMap, 0);

    // Control flow
    if (len > 10.0) {
        vNormal = n * 0.5;
    } else {
        vNormal = n;
    }

    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i == 5) break;
        if (i == 2) continue;
    }

    switch (d) {
        case 0:
            a = 0.0;
            break;
        case 1:
            a = 1.0;
            break;
        default:
            a = -1.0;
            break;
    }

    int x = 0;
    while (x < 10) {
        x++;
    }

    do {
        x--;
    } while (x > 0);

    // Coherent, volatile, restrict, readonly, writeonly
    coherent volatile restrict buffer B2 {
        readonly float rdata;
        writeonly float wdata;
    };

    // Centroid, sample, patch, noperspective, smooth
    centroid out vec4 cColor;
    sample in vec3 sNormal;
    patch in float tessLevel;
    noperspective out float ndcDepth;
    smooth out vec3 smoothNorm;

    // Inout parameter usage (in function context)
    vTexCoord = aTexCoord;
    gl_Position = uProjection * worldPos;

    // String and char literals (C-style, for preprocessor)
    char ch = 'A';
    // A string in a define context: "hello world"
}
