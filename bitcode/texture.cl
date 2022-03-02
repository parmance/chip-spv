// Declarations and implementations for HIP texture functions.
//
// (c) 2022 Henry Linjamäki / Parmance for Argonne National Laboratory

// Texture object type declaration. Must be tandem with
// <Common-HIP>/include/hip/texture_types.h.
struct __hip_texture;
typedef struct __hip_texture *hipTextureObject_t;

// vvv DECLARATIONS INTENTIONALLY WITHOUT DEFINITION vvv

// Texture function lowering pass (HipTextureLowering.cpp) use these
// to recognize the texture function calls in kernels. The pass
// replaces the calls to them with "_chip_*_impl" variant.
float4 _chip_tex1df(hipTextureObject_t TextureObject, int Pos);
float4 _chip_tex2df(hipTextureObject_t TextureObject, float2 Pos);
int4 _chip_tex2di(hipTextureObject_t TextureObject, float2 Pos);
uint4 _chip_tex2du(hipTextureObject_t TextureObject, float2 Pos);

// ^^^ DECLARATIONS INTENTIONALLY WITHOUT DEFINITION ^^^

// TODO: Choose appropriate read_image overload for the image type.
//       The code ahead now assumes that non-normalized image formats
//       are being used.

float4 __attribute__((used))
_chip_tex1df_impl(image1d_t I, sampler_t S, int Pos) {
  return read_imagef(I, S, Pos);
}

float4 __attribute__((used))
_chip_tex2df_impl(image2d_t I, sampler_t S, float2 Pos) {
  return read_imagef(I, S, Pos);
}

int4 __attribute__((used))
_chip_tex2di_impl(image2d_t I, sampler_t S, float2 Pos) {
  return read_imagei(I, S, Pos);
}

uint4 __attribute__((used))
_chip_tex2du_impl(image2d_t I, sampler_t S, float2 Pos) {
  return read_imageui(I, S, Pos);
}
