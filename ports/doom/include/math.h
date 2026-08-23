#ifndef _MATH_H
#define _MATH_H

static inline double fabs(double x) {
    return (x < 0.0) ? -x : x;
}

static inline float fabsf(float x) {
    return (x < 0.0f) ? -x : x;
}

#endif
