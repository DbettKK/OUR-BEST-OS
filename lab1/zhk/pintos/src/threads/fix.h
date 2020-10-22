#ifndef __THREAD_FIX_H
#define __THREAD_FIX_H

#include "thread.h"

#define f (1 << 14)
typedef signed long long int int64_t;

fixed_point integer_convert_to_fixedPoint(fixed_point n);

int fixedPoint_convert_to_integer_roundToZero(fixed_point x);

int fixedPoint_convert_to_integer_roundToNearest(fixed_point x);

fixed_point add_between_fixedPoints(fixed_point x, fixed_point y);

/* substract y from x. */
fixed_point substract_between_fixedPoints(fixed_point x, fixed_point y);

fixed_point add_between_fixedPoints_and_integer(fixed_point x, int n);

/* substract n from x. */
fixed_point substractmultiply(fixed_point x, int n);

fixed_point multiply_between_fixedPoints(fixed_point x, fixed_point y);

fixed_point multiply_between_fixedPoints_and_integer(fixed_point x, int n);

/* divide x by y. */
fixed_point divide_between_fixedPoints(fixed_point x, fixed_point y);

/* divide x by n. */
fixed_point divide_between_fixedPoint_and_integer(fixed_point x, int n);

int check_priority(int pri);

#endif /* thread/fixed_point.h */