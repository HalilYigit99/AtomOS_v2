#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Special values for floating point
#define INFINITY    (__builtin_inff())
#define NAN         (__builtin_nanf(""))
#define HUGE_VAL    (__builtin_inf())
#define HUGE_VALF   (__builtin_inff())
#define HUGE_VALL   (__builtin_infl())

// Mathematical constants
#define M_E        2.71828182845904523536   // e
#define M_LOG2E    1.44269504088896338700   // log_2 e
#define M_LOG10E   0.43429448190325182765   // log_10 e
#define M_LN2      0.69314718055994530942   // log_e 2
#define M_LN10     2.30258509299404568402   // log_e 10
#define M_PI       3.14159265358979323846   // pi
#define M_PI_2     1.57079632679489661923   // pi/2
#define M_PI_4     0.78539816339744830962   // pi/4
#define M_1_PI     0.31830988618379067154   // 1/pi
#define M_2_PI     0.63661977236758134308   // 2/pi
#define M_2_SQRTPI 1.12837916709551257390   // 2/sqrt(pi)
#define M_SQRT2    1.41421356237309504880   // sqrt(2)
#define M_SQRT1_2  0.70710678118654752440   // 1/sqrt(2)

// Basic arithmetic functions
int abs(int x);
long labs(long x);
long long llabs(long long x);
double fabs(double x);
float fabsf(float x);

// Power and exponential functions
double pow(double base, double exponent);
float powf(float base, float exponent);
double sqrt(double x);
float sqrtf(float x);
double exp(double x);
float expf(float x);
double exp2(double x);
float exp2f(float x);
double log(double x);
float logf(float x);
double log2(double x);
float log2f(float x);
double log10(double x);
float log10f(float x);

// Trigonometric functions
double sin(double x);
float sinf(float x);
double cos(double x);
float cosf(float x);
double tan(double x);
float tanf(float x);
double asin(double x);
float asinf(float x);
double acos(double x);
float acosf(float x);
double atan(double x);
float atanf(float x);
double atan2(double y, double x);
float atan2f(float y, float x);

// Hyperbolic functions
double sinh(double x);
float sinhf(float x);
double cosh(double x);
float coshf(float x);
double tanh(double x);
float tanhf(float x);

// Rounding and remainder functions
double ceil(double x);
float ceilf(float x);
double floor(double x);
float floorf(float x);
double round(double x);
float roundf(float x);
double trunc(double x);
float truncf(float x);
double fmod(double x, double y);
float fmodf(float x, float y);
double remainder(double x, double y);
float remainderf(float x, float y);

// Comparison functions
double fmax(double x, double y);
float fmaxf(float x, float y);
double fmin(double x, double y);
float fminf(float x, float y);

// Classification functions
int isfinite(double x);
int isinf(double x);
int isnan(double x);
int isnormal(double x);
int signbit(double x);

// Utility functions
double copysign(double x, double y);
float copysignf(float x, float y);
double ldexp(double x, int exp);
float ldexpf(float x, int exp);
double frexp(double x, int *exp);
float frexpf(float x, int *exp);
double modf(double x, double *iptr);
float modff(float x, float *iptr);

// Fast approximations (useful for kernel/embedded)
double fast_sin(double x);
double fast_cos(double x);
double fast_sqrt(double x);
float fast_sqrtf(float x);

// Integer math utilities
int gcd(int a, int b);
int lcm(int a, int b);
int ipow(int base, int exp);
uint32_t isqrt(uint32_t n);

#ifdef __cplusplus
}
#endif
