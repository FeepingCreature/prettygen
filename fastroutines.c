#include "stdio.h"
#include "math.h"

typedef struct {
  float x, y, z, w;
} __attribute__ ((aligned (16))) vec3f;

typedef struct {
  float x, y;
} __attribute__ ((aligned (4))) vec2f;

typedef float v4sf __attribute__ ((vector_size (16)));
typedef int v4si __attribute__ ((vector_size (16)));

#define ALIGNED __attribute__ ((force_align_arg_pointer))

void ALIGNED fast_memcpy(void* dest, void* src, int n) {
  // fprintf(stderr, "Fast memcpy of %i\n", n);
  __builtin_memcpy(dest, src, n);
}

void ALIGNED fast_sin(float phase, float freq, int count, vec3f *col) {
  for (int i = 0; i < count; ++i) {
    col[i].x = sinf(phase + freq * col[i].x);
    col[i].y = sinf(phase + freq * col[i].y);
    col[i].z = sinf(phase + freq * col[i].z);
  }
}
/*
void ALIGNED fast_supersin(float *phases, float *freqs, int count, Element *res, Element *op2) {
  for (int i = 0; i < count; ++i) {
    res[i].col.x = sinf(phases[0] + freqs[0] * op2[i].col.x);
    res[i].col.y = sinf(phases[1] + freqs[1] * op2[i].col.y);
    res[i].col.z = sinf(phases[2] + freqs[2] * op2[i].col.z);
  }
}

void ALIGNED fast_tent_step(int count, Element *res, Element *op2) {
  for (int i = 0; i < count; ++i) {
    res[i].col.x = 1.0f - 2.0f * fabsf(op2[i].col.x);
    res[i].col.y = 1.0f - 2.0f * fabsf(op2[i].col.y);
    res[i].col.z = 1.0f - 2.0f * fabsf(op2[i].col.z);
  }
}
*/
// Thanks to http://jrfonseca.blogspot.com/2008/09/fast-sse2-pow-tables-or-polynomials.html
#include <xmmintrin.h>
#define EXP_POLY_DEGREE 3

#define POLY0(x, c0) _mm_set1_ps(c0)
#define POLY1(x, c0, c1) _mm_add_ps(_mm_mul_ps(POLY0(x, c1), x), _mm_set1_ps(c0))
#define POLY2(x, c0, c1, c2) _mm_add_ps(_mm_mul_ps(POLY1(x, c1, c2), x), _mm_set1_ps(c0))
#define POLY3(x, c0, c1, c2, c3) _mm_add_ps(_mm_mul_ps(POLY2(x, c1, c2, c3), x), _mm_set1_ps(c0))
#define POLY4(x, c0, c1, c2, c3, c4) _mm_add_ps(_mm_mul_ps(POLY3(x, c1, c2, c3, c4), x), _mm_set1_ps(c0))
#define POLY5(x, c0, c1, c2, c3, c4, c5) _mm_add_ps(_mm_mul_ps(POLY4(x, c1, c2, c3, c4, c5), x), _mm_set1_ps(c0))

static __m128 exp2f4(__m128 x)
{
   __m128i ipart;
   __m128 fpart, expipart, expfpart;

   x = _mm_min_ps(x, _mm_set1_ps( 129.00000f));
   x = _mm_max_ps(x, _mm_set1_ps(-126.99999f));

   /* ipart = int(x - 0.5) */
   ipart = _mm_cvtps_epi32(_mm_sub_ps(x, _mm_set1_ps(0.5f)));

   /* fpart = x - ipart */
   fpart = _mm_sub_ps(x, _mm_cvtepi32_ps(ipart));

   /* expipart = (float) (1 << ipart) */
   expipart = _mm_castsi128_ps(_mm_slli_epi32(_mm_add_epi32(ipart, _mm_set1_epi32(127)), 23));

   /* minimax polynomial fit of 2**x, in range [-0.5, 0.5[ */
#if EXP_POLY_DEGREE == 5
   expfpart = POLY5(fpart, 9.9999994e-1f, 6.9315308e-1f, 2.4015361e-1f, 5.5826318e-2f, 8.9893397e-3f, 1.8775767e-3f);
#elif EXP_POLY_DEGREE == 4
   expfpart = POLY4(fpart, 1.0000026f, 6.9300383e-1f, 2.4144275e-1f, 5.2011464e-2f, 1.3534167e-2f);
#elif EXP_POLY_DEGREE == 3
   expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
#elif EXP_POLY_DEGREE == 2
   expfpart = POLY2(fpart, 1.0017247f, 6.5763628e-1f, 3.3718944e-1f);
#else
#error
#endif

   return _mm_mul_ps(expipart, expfpart);
}

#define LOG_POLY_DEGREE 5

static __m128 log2f4(__m128 x)
{
   __m128i exp = _mm_set1_epi32(0x7F800000);
   __m128i mant = _mm_set1_epi32(0x007FFFFF);

   __m128 one = _mm_set1_ps( 1.0f);

   __m128i i = _mm_castps_si128(x);

   __m128 e = _mm_cvtepi32_ps(_mm_sub_epi32(_mm_srli_epi32(_mm_and_si128(i, exp), 23), _mm_set1_epi32(127)));

   __m128 m = _mm_or_ps(_mm_castsi128_ps(_mm_and_si128(i, mant)), one);

   __m128 p;

   /* Minimax polynomial fit of log2(x)/(x - 1), for x in range [1, 2[ */
#if LOG_POLY_DEGREE == 6
   p = POLY5( m, 3.1157899f, -3.3241990f, 2.5988452f, -1.2315303f,  3.1821337e-1f, -3.4436006e-2f);
#elif LOG_POLY_DEGREE == 5
   p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);
#elif LOG_POLY_DEGREE == 4
   p = POLY3(m, 2.61761038894603480148f, -1.75647175389045657003f, 0.688243882994381274313f, -0.107254423828329604454f);
#elif LOG_POLY_DEGREE == 3
   p = POLY2(m, 2.28330284476918490682f, -1.04913055217340124191f, 0.204446009836232697516f);
#else
#error
#endif

   /* This effectively increases the polynomial degree by one, but ensures that log2(1) == 0*/
   p = _mm_mul_ps(p, _mm_sub_ps(m, one));

   return _mm_add_ps(p, e);
}

void ALIGNED fast_pow_vec(__m128 *x, __m128 *y, __m128 *out)
{
   *out = exp2f4(_mm_mul_ps(log2f4(*x), *y));
}
