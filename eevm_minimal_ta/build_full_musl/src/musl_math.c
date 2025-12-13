// Math function stubs for libcxx
typedef __SIZE_TYPE__ size_t;
typedef unsigned wchar_t;

#define NAN __builtin_nanf("")
#define INFINITY __builtin_inff()

// Basic math functions
double fabs(double x) { return x < 0 ? -x : x; }
float fabsf(float x) { return x < 0 ? -x : x; }
long double fabsl(long double x) { return x < 0 ? -x : x; }

int isnan(double x) { return x != x; }
int isinf(double x) { return fabs(x) == INFINITY; }
int isfinite(double x) { return !isnan(x) && !isinf(x); }
int isnormal(double x) { return isfinite(x) && x != 0.0; }

// Trigonometric stubs (return 0 or NAN)
double sin(double x) { return 0.0; }
double cos(double x) { return 1.0; }
double tan(double x) { return 0.0; }
double asin(double x) { return NAN; }
double acos(double x) { return NAN; }
double atan(double x) { return NAN; }
double atan2(double y, double x) { return NAN; }

float sinf(float x) { return 0.0f; }
float cosf(float x) { return 1.0f; }
float tanf(float x) { return 0.0f; }
float asinf(float x) { return NAN; }
float acosf(float x) { return NAN; }
float atanf(float x) { return NAN; }
float atan2f(float y, float x) { return NAN; }

// Exponential/logarithmic stubs
double exp(double x) { return NAN; }
double log(double x) { return NAN; }
double log10(double x) { return NAN; }
double pow(double x, double y) { return NAN; }
double sqrt(double x) { return NAN; }

float expf(float x) { return NAN; }
float logf(float x) { return NAN; }
float log10f(float x) { return NAN; }
float powf(float x, float y) { return NAN; }
float sqrtf(float x) { return NAN; }

// Hyperbolic stubs
double sinh(double x) { return NAN; }
double cosh(double x) { return NAN; }
double tanh(double x) { return NAN; }
double asinh(double x) { return NAN; }
double acosh(double x) { return NAN; }
double atanh(double x) { return NAN; }

// Rounding functions
double ceil(double x) { return (double)((long long)x + (x > 0 && x != (long long)x)); }
double floor(double x) { return (double)((long long)x - (x < 0 && x != (long long)x)); }
double trunc(double x) { return (double)((long long)x); }
double round(double x) { return floor(x + 0.5); }

float ceilf(float x) { return (float)ceil((double)x); }
float floorf(float x) { return (float)floor((double)x); }
float truncf(float x) { return (float)trunc((double)x); }
float roundf(float x) { return (float)round((double)x); }

// Remainder functions  
double fmod(double x, double y) { return x - trunc(x / y) * y; }
double remainder(double x, double y) { return fmod(x, y); }
double remquo(double x, double y, int* quo) { *quo = (int)(x / y); return fmod(x, y); }

// More stubs (add as needed when compiler complains)
double copysign(double x, double y) { return (y < 0) ? -fabs(x) : fabs(x); }
double scalbn(double x, int n) { return x; }
double ldexp(double x, int exp) { return x; }
double frexp(double x, int* exp) { *exp = 0; return x; }
double modf(double x, double* iptr) { *iptr = trunc(x); return x - *iptr; }

float copysignf(float x, float y) { return (float)copysign((double)x, (double)y); }
float scalbnf(float x, int n) { return x; }
float ldexpf(float x, int exp) { return x; }
float frexpf(float x, int* exp) { *exp = 0; return x; }
float modff(float x, float* iptr) { double d; float r = (float)modf((double)x, &d); *iptr = (float)d; return r; }

// Long double versions (just cast to double)
long double sinl(long double x) { return (long double)sin((double)x); }
long double cosl(long double x) { return (long double)cos((double)x); }
long double tanl(long double x) { return (long double)tan((double)x); }
long double asinl(long double x) { return (long double)asin((double)x); }
long double acosl(long double x) { return (long double)acos((double)x); }
long double atanl(long double x) { return (long double)atan((double)x); }
long double atan2l(long double y, long double x) { return (long double)atan2((double)y, (double)x); }
long double expl(long double x) { return (long double)exp((double)x); }
long double logl(long double x) { return (long double)log((double)x); }
long double log10l(long double x) { return (long double)log10((double)x); }
long double powl(long double x, long double y) { return (long double)pow((double)x, (double)y); }
long double sqrtl(long double x) { return (long double)sqrt((double)x); }
long double ceill(long double x) { return (long double)ceil((double)x); }
long double floorl(long double x) { return (long double)floor((double)x); }
long double truncl(long double x) { return (long double)trunc((double)x); }
long double roundl(long double x) { return (long double)round((double)x); }
long double fmodl(long double x, long double y) { return (long double)fmod((double)x, (double)y); }
long double copysignl(long double x, long double y) { return (long double)copysign((double)x, (double)y); }

// Even more stubs that libcxx might reference
double exp2(double x) { return pow(2.0, x); }
double log2(double x) { return log(x) / log(2.0); }
double cbrt(double x) { return pow(x, 1.0/3.0); }
double hypot(double x, double y) { return sqrt(x*x + y*y); }
double erf(double x) { return NAN; }
double erfc(double x) { return NAN; }
double tgamma(double x) { return NAN; }
double lgamma(double x) { return NAN; }

float exp2f(float x) { return (float)exp2((double)x); }
float log2f(float x) { return (float)log2((double)x); }
float cbrtf(float x) { return (float)cbrt((double)x); }
float hypotf(float x, float y) { return (float)hypot((double)x, (double)y); }
float erff(float x) { return NAN; }
float erfcf(float x) { return NAN; }
float tgammaf(float x) { return NAN; }
float lgammaf(float x) { return NAN; }

long double exp2l(long double x) { return (long double)exp2((double)x); }
long double log2l(long double x) { return (long double)log2((double)x); }
long double cbrtl(long double x) { return (long double)cbrt((double)x); }
long double hypotl(long double x, long double y) { return (long double)hypot((double)x, (double)y); }
long double erfl(long double x) { return NAN; }
long double erfcl(long double x) { return NAN; }
long double tgammal(long double x) { return NAN; }
long double lgammal(long double x) { return NAN; }

// Classification functions
int fpclassify(double x) { return isnormal(x) ? 3 : (x == 0.0 ? 5 : 0); }
int signbit(double x) { return x < 0; }
