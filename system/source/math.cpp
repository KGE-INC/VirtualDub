#include <math.h>
#include <vd2/system/math.h>

int VDRoundToInt(double x) {
	return (int)floor(x + 0.5);
}

long VDRoundToLong(double x) {
	return (long)floor(x + 0.5);
}
