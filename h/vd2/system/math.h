#ifndef f_VD2_MATH_H
#define f_VD2_MATH_H

// Rounding functions
//
// Round a double to an int or a long.  Behavior is not specified at
// int(y)+0.5, if x is NaN or Inf, or if x is out of range.

int VDRoundToInt(double x);
long VDRoundToLong(double x);

#endif
