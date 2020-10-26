#include "threads/self-compute.h"
#include <stdio.h>

fixed_point integer_convert_to_fixedPoint(fixed_point n) {
    return n * f;
}

int fixedPoint_convert_to_integer_roundToZero(fixed_point x) {
    return x / f;
}

int fixedPoint_convert_to_integer_roundToNearest(fixed_point x) {
    return x <= 0 ? (x - f / 2) / f : (x + f / 2) / f;
}

fixed_point add_between_fixedPoints(fixed_point x, fixed_point y) {
    return x + y;
}

/* substract y from x. */
fixed_point substract_between_fixedPoints(fixed_point x, fixed_point y) {
    return x - y;
}

fixed_point add_between_fixedPoints_and_integer(fixed_point x, int n){
    return x + n * f;
}

/* substract n from x. */
fixed_point substractmultiply(fixed_point x, int n) {
    return x - n * f;
}

fixed_point multiply_between_fixedPoints(fixed_point x, fixed_point y) {
    return ((int64_t) x) * y / f;
}

fixed_point multiply_between_fixedPoints_and_integer(fixed_point x, int n) {
    return x * n;
}

/* divide x by y. */
fixed_point divide_between_fixedPoints(fixed_point x, fixed_point y) {
    return ((int64_t) x) * f / y;
}

/* divide x by n. */
fixed_point divide_between_fixedPoint_and_integer(fixed_point x, int n) {
    return x / n;
}

int check_priority(int pri) {
    if (pri < PRI_MIN) {
        return PRI_MIN;
    } else if (pri > PRI_MAX) {
        return PRI_MAX;
    } else {
        return pri;
    }
}
