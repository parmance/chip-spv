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

extern "C" __device__ _native_float4
_chip_tex1df(hipTextureObject_t textureObject, int pos);

extern "C" __device__ _native_float4
_chip_tex2df(hipTextureObject_t textureObject, _native_float2 pos);

extern "C" __device__ _native_int4
_chip_tex2di(hipTextureObject_t textureObject, _native_float2 pos);

extern "C" __device__ _native_uint4
_chip_tex2du(hipTextureObject_t textureObject, _native_float2 pos);

__TEXTURE_FUNCTIONS_DECL__ void
tex1Dfetch(float *retVal, hipTextureObject_t textureObject, int x) {
  *retVal = _chip_tex1df(textureObject, x).x;
}

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

template <class T>
__TEXTURE_FUNCTIONS_DECL__ T tex1Dfetch(hipTextureObject_t textureObject,
                                        int x) {
  T ret;
  tex1Dfetch(&ret, textureObject, x);
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
