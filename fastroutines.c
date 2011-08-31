#include "stdio.h"
#include "math.h"

typedef struct {
  float x, y, z, w;
} __attribute__ ((aligned (16))) vec3f;

typedef struct {
  float x, y;
} __attribute__ ((aligned (4))) vec2f;

typedef struct {
  vec2f pos;
  vec3f col;
} Element;

typedef float v4sf __attribute__ ((vector_size (16)));
typedef int v4si __attribute__ ((vector_size (16)));

#define ALIGNED __attribute__ ((force_align_arg_pointer))

void ALIGNED fast_sine_step(float phase, float freq, int count, Element *res, Element *op2) {
  for (int i = 0; i < count; ++i) {
    res[i].col.x = __builtin_sinf(phase + freq * op2[i].col.x);
    res[i].col.y = __builtin_sinf(phase + freq * op2[i].col.y);
    res[i].col.z = __builtin_sinf(phase + freq * op2[i].col.z);
  }
}
