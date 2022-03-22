/*
Copyright (c) 2015 - 2021 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef SPIRV_HIP_TEXTURE_FUNCTIONS_H
#define SPIRV_HIP_TEXTURE_FUNCTIONS_H

#include "spirv_hip_vector_types.h"

#define __TEXTURE_FUNCTIONS_DECL__ static inline __device__

// TODO: Test in C compilation mode.

// TODO: Move to vector header?
typedef float _native_float2 __attribute__((ext_vector_type(2)));
typedef float _native_float4 __attribute__((ext_vector_type(4)));
typedef int _native_int4 __attribute__((ext_vector_type(4)));
typedef unsigned int _native_uint4 __attribute__((ext_vector_type(4)));

extern "C" __device__ _native_int4
_chip_tex1dfetchi(hipTextureObject_t textureObject, int pos);

extern "C" __device__ _native_uint4
_chip_tex1dfetchu(hipTextureObject_t textureObject, int pos);

extern "C" __device__ _native_float4
_chip_tex1dfetchf(hipTextureObject_t textureObject, int pos);

extern "C" __device__ _native_int4
_chip_tex1di(hipTextureObject_t textureObject, float pos);

extern "C" __device__ _native_uint4
_chip_tex1du(hipTextureObject_t textureObject, float pos);

extern "C" __device__ _native_float4
_chip_tex1df(hipTextureObject_t textureObject, float pos);

extern "C" __device__ _native_float4
_chip_tex2df(hipTextureObject_t textureObject, _native_float2 pos);

extern "C" __device__ _native_int4
_chip_tex2di(hipTextureObject_t textureObject, _native_float2 pos);

extern "C" __device__ _native_uint4
_chip_tex2du(hipTextureObject_t textureObject, _native_float2 pos);

#define DEF_TEX1D_SCL(_NAME, _RES_TY, _POS_TY, _IFN)                           \
  __TEXTURE_FUNCTIONS_DECL__ void _NAME(                                       \
      _RES_TY *retVal, hipTextureObject_t textureObject, _POS_TY x) {          \
    *retVal = _IFN(textureObject, x).x;                                        \
  }                                                                            \
  __asm__("")

#define DEF_TEX1D_VEC1(_NAME, _RES_TY, _POS_TY, _IFN)                          \
  __TEXTURE_FUNCTIONS_DECL__ void _NAME(                                       \
      _RES_TY *retVal, hipTextureObject_t textureObject, _POS_TY x) {          \
    auto res = _IFN(textureObject, x);                                         \
    *retVal = make_##_RES_TY(res.x);                                           \
  }                                                                            \
  __asm__("")

#define DEF_TEX1D_VEC2(_NAME, _RES_TY, _POS_TY, _IFN)                          \
  __TEXTURE_FUNCTIONS_DECL__ void _NAME(                                       \
      _RES_TY *retVal, hipTextureObject_t textureObject, _POS_TY x) {          \
    auto res = _IFN(textureObject, x);                                         \
    *retVal = make_##_RES_TY(res.x, res.y);                                    \
  }                                                                            \
  __asm__("")

#define DEF_TEX1D_VEC4(_NAME, _RES_TY, _POS_TY, _IFN)                          \
  __TEXTURE_FUNCTIONS_DECL__ void _NAME(                                       \
      _RES_TY *retVal, hipTextureObject_t textureObject, _POS_TY x) {          \
    auto res = _IFN(textureObject, x);                                         \
    *retVal = make_##_RES_TY(res.x, res.y, res.z, res.w);                      \
  }                                                                            \
  __asm__("")

// tex1DFetch //

DEF_TEX1D_SCL(tex1Dfetch, char, int, _chip_tex1dfetchi);
DEF_TEX1D_SCL(tex1Dfetch, unsigned char, int, _chip_tex1dfetchu);
DEF_TEX1D_SCL(tex1Dfetch, short, int, _chip_tex1dfetchi);
DEF_TEX1D_SCL(tex1Dfetch, unsigned short, int, _chip_tex1dfetchu);
DEF_TEX1D_SCL(tex1Dfetch, int, int, _chip_tex1dfetchi);
DEF_TEX1D_SCL(tex1Dfetch, unsigned int, int, _chip_tex1dfetchu);
DEF_TEX1D_SCL(tex1Dfetch, float, int, _chip_tex1dfetchf);

DEF_TEX1D_VEC1(tex1Dfetch, char1, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC1(tex1Dfetch, uchar1, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC1(tex1Dfetch, short1, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC1(tex1Dfetch, ushort1, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC1(tex1Dfetch, int1, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC1(tex1Dfetch, uint1, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC1(tex1Dfetch, float1, int, _chip_tex1dfetchf);

DEF_TEX1D_VEC2(tex1Dfetch, char2, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC2(tex1Dfetch, uchar2, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC2(tex1Dfetch, short2, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC2(tex1Dfetch, ushort2, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC2(tex1Dfetch, int2, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC2(tex1Dfetch, uint2, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC2(tex1Dfetch, float2, int, _chip_tex1dfetchf);

DEF_TEX1D_VEC4(tex1Dfetch, char4, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC4(tex1Dfetch, uchar4, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC4(tex1Dfetch, short4, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC4(tex1Dfetch, ushort4, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC4(tex1Dfetch, int4, int, _chip_tex1dfetchi);
DEF_TEX1D_VEC4(tex1Dfetch, uint4, int, _chip_tex1dfetchu);
DEF_TEX1D_VEC4(tex1Dfetch, float4, int, _chip_tex1dfetchf);

// tex1D //

DEF_TEX1D_SCL(tex1D, char, float, _chip_tex1di);
DEF_TEX1D_SCL(tex1D, unsigned char, float, _chip_tex1du);
DEF_TEX1D_SCL(tex1D, short, float, _chip_tex1di);
DEF_TEX1D_SCL(tex1D, unsigned short, float, _chip_tex1du);
DEF_TEX1D_SCL(tex1D, int, float, _chip_tex1di);
DEF_TEX1D_SCL(tex1D, unsigned int, float, _chip_tex1du);
DEF_TEX1D_SCL(tex1D, float, float, _chip_tex1df);

DEF_TEX1D_VEC1(tex1D, char1, float, _chip_tex1di);
DEF_TEX1D_VEC1(tex1D, uchar1, float, _chip_tex1du);
DEF_TEX1D_VEC1(tex1D, short1, float, _chip_tex1di);
DEF_TEX1D_VEC1(tex1D, ushort1, float, _chip_tex1du);
DEF_TEX1D_VEC1(tex1D, int1, float, _chip_tex1di);
DEF_TEX1D_VEC1(tex1D, uint1, float, _chip_tex1du);
DEF_TEX1D_VEC1(tex1D, float1, float, _chip_tex1df);

DEF_TEX1D_VEC2(tex1D, char2, float, _chip_tex1di);
DEF_TEX1D_VEC2(tex1D, uchar2, float, _chip_tex1du);
DEF_TEX1D_VEC2(tex1D, short2, float, _chip_tex1di);
DEF_TEX1D_VEC2(tex1D, ushort2, float, _chip_tex1du);
DEF_TEX1D_VEC2(tex1D, int2, float, _chip_tex1di);
DEF_TEX1D_VEC2(tex1D, uint2, float, _chip_tex1du);
DEF_TEX1D_VEC2(tex1D, float2, float, _chip_tex1df);

DEF_TEX1D_VEC4(tex1D, char4, float, _chip_tex1di);
DEF_TEX1D_VEC4(tex1D, uchar4, float, _chip_tex1du);
DEF_TEX1D_VEC4(tex1D, short4, float, _chip_tex1di);
DEF_TEX1D_VEC4(tex1D, ushort4, float, _chip_tex1du);
DEF_TEX1D_VEC4(tex1D, int4, float, _chip_tex1di);
DEF_TEX1D_VEC4(tex1D, uint4, float, _chip_tex1du);
DEF_TEX1D_VEC4(tex1D, float4, float, _chip_tex1df);


// tex2D //

__TEXTURE_FUNCTIONS_DECL__ void
tex2D(char *retVal, hipTextureObject_t textureObject, float x, float y) {
  _native_float2 pos;
  pos.x = x;
  pos.y = y;
  *retVal = _chip_tex2di(textureObject, pos).x;
}

__TEXTURE_FUNCTIONS_DECL__ void
tex2D(short *retVal, hipTextureObject_t textureObject, float x, float y) {
  _native_float2 pos;
  pos.x = x;
  pos.y = y;
  *retVal = _chip_tex2di(textureObject, pos).x;
}

__TEXTURE_FUNCTIONS_DECL__ void
tex2D(int *retVal, hipTextureObject_t textureObject, float x, float y) {
  _native_float2 pos;
  pos.x = x;
  pos.y = y;
  *retVal = _chip_tex2di(textureObject, pos).x;
}

__TEXTURE_FUNCTIONS_DECL__ void tex2D(unsigned char *retVal,
                                      hipTextureObject_t textureObject, float x,
                                      float y) {
  _native_float2 pos;
  pos.x = x;
  pos.y = y;
  *retVal = _chip_tex2du(textureObject, pos).x;
}

__TEXTURE_FUNCTIONS_DECL__ void tex2D(unsigned short *retVal,
                                      hipTextureObject_t textureObject, float x,
                                      float y) {
  _native_float2 pos;
  pos.x = x;
  pos.y = y;
  *retVal = _chip_tex2du(textureObject, pos).x;
}

__TEXTURE_FUNCTIONS_DECL__ void
tex2D(unsigned *retVal, hipTextureObject_t textureObject, float x, float y) {
  _native_float2 pos;
  pos.x = x;
  pos.y = y;
  *retVal = _chip_tex2du(textureObject, pos).x;
}

__TEXTURE_FUNCTIONS_DECL__ void
tex2D(float *retVal, hipTextureObject_t textureObject, float x, float y) {
  _native_float2 pos;
  pos.x = x;
  pos.y = y;
  *retVal = _chip_tex2df(textureObject, pos).x;
}

// Public HIP Runtime API Bindings //

template <class T>
__TEXTURE_FUNCTIONS_DECL__ T tex1Dfetch(hipTextureObject_t textureObject,
                                        int x) {
  T ret;
  tex1Dfetch(&ret, textureObject, x);
  return ret;
}

template <class T>
__TEXTURE_FUNCTIONS_DECL__ T tex1D(hipTextureObject_t textureObject, float x) {
  T ret;
  tex1D(&ret, textureObject, x);
  return ret;
}

template <class T>
__TEXTURE_FUNCTIONS_DECL__ T tex2D(hipTextureObject_t textureObject, float x,
                                   float y) {
  T ret;
  tex2D(&ret, textureObject, x, y);
  return ret;
}

#endif
