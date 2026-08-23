/* !DEFINES!

$define %func svg_sin as function with args double
$define %func svg_cos as function with args double
$define %func svg_sqrt as function with args double
$define %func svg_atan2 as function with args double, double
$define %func svg_acos as function with args double
$define %func svg_strtod as function with args const char *, char **

*/

/* !SPACE!

$space %internal svg_fmod_abs, svg_atan_unit, svg_range_reduce

*/

#include <svg_int.h>

static double
svg_fmod_abs(double x, double period)
{
	double	reduced;

	reduced = x;
	while (reduced >= period) {
		reduced -= period;
	}
	return (reduced);
}

static double
svg_sin_core(double x)
{
	double	x2, term, sum;
	int	i;

	x2 = x * x;
	term = x;
	sum = x;
	for (i = 1; i < 12; i++) {
		term *= -x2 / ((2.0 * i) * (2.0 * i + 1.0));
		sum += term;
		if (term > -1e-15 && term < 1e-15) {
			break;
		}
	}
	return (sum);
}

static double
svg_range_reduce(double x, int *out_sign)
{
	double	mag, two_pi = 6.28318530717958648;
	double	half_pi = 1.57079632679489662;

	if (x < 0.0) {
		*out_sign = -1;
		mag = -x;
	} else {
		*out_sign = 1;
		mag = x;
	}
	mag = svg_fmod_abs(mag, two_pi);
	if (mag > half_pi && mag <= 3.0 * half_pi) {
		mag = 2.0 * half_pi - mag;
	} else if (mag > 3.0 * half_pi) {
		mag -= two_pi;
		if (mag < 0.0) {
			mag = -mag;
			*out_sign = -*out_sign;
		}
	}
	return (mag);
}

double
svg_sin(double x)
{
	int	sign;

	sign = 1;
	x = svg_range_reduce(x, &sign);
	return ((double)sign * svg_sin_core(x));
}

double
svg_cos(double x)
{
	int	sign;

	sign = 1;
	x = svg_range_reduce(x + 1.57079632679489662, &sign);
	return ((double)sign * svg_sin_core(x));
}

double
svg_sqrt(double x)
{
	double	guess;
	int	i, k;

	if (x <= 0.0) {
		return (0.0);
	}

	k = 0;
	while (x > 2.0) {
		x *= 0.25;
		k++;
	}
	while (x < 0.5) {
		x *= 4.0;
		k--;
	}
	guess = x * 0.5 + 0.5;
	for (i = 0; i < 32; i++) {
		guess = 0.5 * (guess + x / guess);
	}
	while (k > 0) {
		guess *= 2.0;
		k--;
	}
	while (k < 0) {
		guess *= 0.5;
		k++;
	}
	return (guess);
}

static double
svg_atan_unit(double z)
{
	double	z2, term, sum;
	int	i, negate;

	negate = 0;
	if (z > 1.0) {
		z = 1.0 / z;
		negate = 1;
	} else if (z < -1.0) {
		z = 1.0 / z;
		negate = 1;
	}

	if (z > 0.5 || z < -0.5) {
		double	t = (z - 1.0) / (z + 1.0);
		z = t;
		sum = 0.78539816339744831;
	} else {
		sum = 0.0;
	}

	z2 = z * z;
	term = z;
	sum += z;
	for (i = 1; i < 30; i++) {
		term *= -z2;
		sum += term / (2.0 * i + 1.0);
		if (term < 1e-14 && term > -1e-14) {
			break;
		}
	}
	if (negate != 0) {
		sum = 1.57079632679489662 - sum;
	}
	return (sum);
}

double
svg_atan2(double y, double x)
{
	if (x == 0.0 && y == 0.0) {
		return (0.0);
	}
	if (x == 0.0) {
		return ((y > 0.0) ? 1.57079632679489662 :
		    -1.57079632679489662);
	}
	if (x > 0.0) {
		return (svg_atan_unit(y / x));
	}
	return ((y >= 0.0) ? svg_atan_unit(y / x) + 3.14159265358979324 :
	    svg_atan_unit(y / x) - 3.14159265358979324);
}

double
svg_acos(double x)
{
	if (x >= 1.0) {
		return (0.0);
	}
	if (x <= -1.0) {
		return (3.14159265358979324);
	}
	return (svg_atan2(svg_sqrt(1.0 - x * x), x));
}

double
svg_strtod(const char *str, char **endptr)
{
	const char	*p;
	double		result, frac;
	int		negative, seen_digit, seen_exp, exp_negative;
	int		exp_value;

	if (str == NULL) {
		if (endptr != NULL) {
			*endptr = NULL;
		}
		return (0.0);
	}

	p = str;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
		p++;
	}

	negative = 0;
	if (*p == '+') {
		p++;
	} else if (*p == '-') {
		negative = 1;
		p++;
	}

	result = 0.0;
	frac = 0.0;
	seen_digit = 0;
	exp_value = 0;
	exp_negative = 0;
	seen_exp = 0;

	while (*p >= '0' && *p <= '9') {
		result = result * 10.0 + (double)(*p - '0');
		seen_digit = 1;
		p++;
	}
	if (*p == '.') {
		p++;
		while (*p >= '0' && *p <= '9') {
			frac += (double)(*p - '0');
			frac *= 0.1;
			seen_digit = 1;
			p++;
		}
		result += frac;
	}
	if (!seen_digit) {
		if (endptr != NULL) {
			*endptr = (char *)str;
		}
		return (0.0);
	}

	if (*p == 'e' || *p == 'E') {
		const char	*q;

		q = p + 1;
		if (*q == '+') {
			q++;
		} else if (*q == '-') {
			exp_negative = 1;
			q++;
		}
		if (*q >= '0' && *q <= '9') {
			seen_exp = 1;
			while (*q >= '0' && *q <= '9') {
				if (exp_value < 10000) {
					exp_value = exp_value * 10 +
					    (*q - '0');
				}
				q++;
			}
			p = q;
		}
		(void)seen_exp;
	}

	while (exp_value > 0) {
		if (exp_negative != 0) {
			result *= 0.1;
		} else {
			result *= 10.0;
		}
		exp_value--;
	}

	if (negative != 0) {
		result = -result;
	}
	if (endptr != NULL) {
		*endptr = (char *)p;
	}
	return (result);
}
