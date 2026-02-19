#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC_WRAP 4
#define ANISOTROPIC_CLAMP 5

SamplerState samplerStates[ANISOTROPIC_CLAMP] : register(s0);
