#pragma once

#ifndef AVES__DTOA_CONFIG_H
#define AVES__DTOA_CONFIG_H

#include <stdint.h>
#define IEEE_8087
#define NO_LONG_LONG
#define NO_HEX_FP

// From dtoa.c, plus extra spacing for readability:
/*
 * #define IEEE_8087 for IEEE-arithmetic machines where the least
 *	  significant byte has the lowest address.
 *
 * #define IEEE_MC68k for IEEE-arithmetic machines where the most
 *	  significant byte has the lowest address.
 *
 * #define Long int on machines with 32-bit ints and 64-bit longs.
 *
 * #define IBM for IBM mainframe-style floating-point arithmetic.
 *
 * #define VAX for VAX-style floating-point arithmetic (D_floating).
 *
 * #define No_leftright to omit left-right logic in fast floating-point
 *	  computation of dtoa.  This will cause dtoa modes 4 and 5 to be
 *	  treated the same as modes 2 and 3 for some inputs.
 *
 * #define Honor_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3
 *	  and strtod and dtoa should round accordingly.  Unless Trust_FLT_ROUNDS
 *	  is also #defined, fegetround() will be queried for the rounding mode.
 *	  Note that both FLT_ROUNDS and fegetround() are specified by the C99
 *	  standard (and are specified to be consistent, with fesetround()
 *	  affecting the value of FLT_ROUNDS), but that some (Linux) systems
 *	  do not work correctly in this regard, so using fegetround() is more
 *	  portable than using FLT_ROUNDS directly.
 *
 * #define Check_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3
 *	  and Honor_FLT_ROUNDS is not #defined.
 *
 * #define RND_PRODQUOT to use rnd_prod and rnd_quot (assembly routines
 *	  that use extended-precision instructions to compute rounded
 *	  products and quotients) with IBM.
 *
 * #define ROUND_BIASED for IEEE-format with biased rounding and arithmetic
 *	  that rounds toward +Infinity.
 *
 * #define ROUND_BIASED_without_Round_Up for IEEE-format with biased
 *	  rounding when the underlying floating-point arithmetic uses
 *	  unbiased rounding.  This prevent using ordinary floating-point
 *	  arithmetic when the result could be computed with one rounding error.
 *
 * #define Inaccurate_Divide for IEEE-format with correctly rounded
 *	  products but inaccurate quotients, e.g., for Intel i860.
 *
 * #define NO_LONG_LONG on machines that do not have a "long long"
 *	  integer type (of >= 64 bits).  On such machines, you can
 *	  #define Just_16 to store 16 bits per 32-bit Long when doing
 *	  high-precision integer arithmetic.  Whether this speeds things
 *	  up or slows things down depends on the machine and the number
 *	  being converted.  If long long is available and the name is
 *	  something other than "long long", #define Llong to be the name,
 *	  and if "unsigned Llong" does not work as an unsigned version of
 *	  Llong, #define #ULLong to be the corresponding unsigned type.
 *
 * #define Bad_float_h if your system lacks a float.h or if it does not
 *	  define some or all of DBL_DIG, DBL_MAX_10_EXP, DBL_MAX_EXP,
 *	  FLT_RADIX, FLT_ROUNDS, and DBL_MAX.
 *
 * #define MALLOC your_malloc, where your_malloc(n) acts like malloc(n)
 *	  if memory is available and otherwise does something you deem
 *	  appropriate.  If MALLOC is undefined, malloc will be invoked
 *	  directly -- and assumed always to succeed.  Similarly, if you
 *	  want something other than the system's free() to be called to
 *	  recycle memory acquired from MALLOC, #define FREE to be the
 *	  name of the alternate routine.  (FREE or free is only called in
 *	  pathological cases, e.g., in a dtoa call after a dtoa return in
 *	  mode 3 with thousands of digits requested.)
 *
 * #define Omit_Private_Memory to omit logic (added Jan. 1998) for making
 *	  memory allocations from a private pool of memory when possible.
 *	  When used, the private pool is PRIVATE_MEM bytes long:  2304 bytes,
 *	  unless #defined to be a different length.  This default length
 *	  suffices to get rid of MALLOC calls except for unusual cases,
 *	  such as decimal-to-binary conversion of a very long string of
 *	  digits.  The longest string dtoa can return is about 751 bytes
 *	  long.  For conversions by strtod of strings of 800 digits and
 *	  all dtoa conversions in single-threaded executions with 8-byte
 *	  pointers, PRIVATE_MEM >= 7400 appears to suffice; with 4-byte
 *	  pointers, PRIVATE_MEM >= 7112 appears adequate.
 *
 * #define NO_INFNAN_CHECK if you do not wish to have INFNAN_CHECK
 *	  #defined automatically on IEEE systems.  On such systems,
 *	  when INFNAN_CHECK is #defined, strtod checks
 *	  for Infinity and NaN (case insensitively).  On some systems
 *	  (e.g., some HP systems), it may be necessary to #define NAN_WORD0
 *	  appropriately -- to the most significant word of a quiet NaN.
 *	  (On HP Series 700/800 machines, -DNAN_WORD0=0x7ff40000 works.)
 *	  When INFNAN_CHECK is #defined and No_Hex_NaN is not #defined,
 *	  strtod also accepts (case insensitively) strings of the form
 *	  NaN(x), where x is a string of hexadecimal digits and spaces;
 *	  if there is only one string of hexadecimal digits, it is taken
 *	  for the 52 fraction bits of the resulting NaN; if there are two
 *	  or more strings of hex digits, the first is for the high 20 bits,
 *	  the second and subsequent for the low 32 bits, with intervening
 *	  white space ignored; but if this results in none of the 52
 *	  fraction bits being on (an IEEE Infinity symbol), then NAN_WORD0
 *	  and NAN_WORD1 are used instead.
 *
 * #define MULTIPLE_THREADS if the system offers preemptively scheduled
 *	  multiple threads.  In this case, you must provide (or suitably
 *	  #define) two locks, acquired by ACQUIRE_DTOA_LOCK(n) and freed
 *	  by FREE_DTOA_LOCK(n) for n = 0 or 1.  (The second lock, accessed
 *	  in pow5mult, ensures lazy evaluation of only one copy of high
 *	  powers of 5; omitting this lock would introduce a small
 *	  probability of wasting memory, but would otherwise be harmless.)
 *	  You must also invoke freedtoa(s) to free the value s returned by
 *	  dtoa.  You may do so whether or not MULTIPLE_THREADS is #defined.
 *
 * #define NO_IEEE_Scale to disable new (Feb. 1997) logic in strtod that
 *	  avoids underflows on inputs whose result does not underflow.
 *	  If you #define NO_IEEE_Scale on a machine that uses IEEE-format
 *	  floating-point numbers and flushes underflows to zero rather
 *	  than implementing gradual underflow, then you must also #define
 *	  Sudden_Underflow.
 *
 * #define SET_INEXACT if IEEE arithmetic is being used and extra
 *	  computation should be done to set the inexact flag when the
 *	  result is inexact and avoid setting inexact when the result
 *	  is exact.  In this case, dtoa.c must be compiled in
 *	  an environment, perhaps provided by #include "dtoa.c" in a
 *	  suitable wrapper, that defines two functions,
 *	      int get_inexact(void);
 *	      void clear_inexact(void);
 *	  such that get_inexact() returns a nonzero value if the
 *	  inexact bit is already set, and clear_inexact() sets the
 *	  inexact bit to 0.  When SET_INEXACT is #defined, strtod
 *	  also does extra computations to set the underflow and overflow
 *	  flags when appropriate (i.e., when the result is tiny and
 *	  inexact or when it is a numeric value rounded to +-infinity).
 *
 * #define NO_ERRNO if strtod should not assign errno = ERANGE when
 *	  the result overflows to +-Infinity or underflows to 0.
 *
 * #define NO_HEX_FP to omit recognition of hexadecimal floating-point
 *	  values by strtod.
 *
 * #define NO_STRTOD_BIGCOMP (on IEEE-arithmetic systems only for now)
 *	  to disable logic for "fast" testing of very long input strings
 *	  to strtod.  This testing proceeds by initially truncating the
 *	  input string, then if necessary comparing the whole string with
 *	  a decimal expansion to decide close cases. This logic is only
 *	  used for input more than STRTOD_DIGLIM digits long (default 40).
 */

// Floating-point formatting modes
// (documentation taken from dtoa)
enum FloatingPointMode
{
	// shortest string that yields d when read in
	// and rounded to nearest.
	FPM_SHORTEST = 0,
	// like 0, but with Steele & White stopping rule;
	// e.g. with IEEE P754 arithmetic, mode 0 gives
	// 1e23 whereas mode 1 gives 9.999999999999999e22.
	FPM_SHORTEST_WITH_STOP = 1,
    // max(1,ndigits) significant digits. This gives
    // a return value similar to that of ecvt, except
    // that trailing zeros are suppressed.
	FPM_MAX_SIGNIFICANT = 2,
	// through ndigits past the decimal point. This
	// gives a return value similar to that from fcvt,
	// except that trailing zeros are suppressed, and
	// ndigits can be negative.
	FPM_MAX_DECIMALS = 3,
	// similar to 2 and 3, respectively, but (in
	// round-nearest mode) with the tests of mode 0 to
	// possibly return a shorter string that rounds to d.
	// With IEEE arithmetic and compilation with
	// -DHonor_FLT_ROUNDS, modes 4 and 5 behave the same
	// as modes 2 and 3 when FLT_ROUNDS != 1.
	FPM_MAX_SIGNIFICANT_ROUNDED = 4,
	FPM_MAX_DECIMALS_ROUNDED = 5,
	// (the rest are debug modes)
};

#ifdef __cplusplus
extern "C" {
#endif

double _aves_strtod(const char *s00, char **se);
// Parameters:
//    d:       The number to convert
//    mode:    One of the enum FloatingPointMode values
//    ndigits: Number of significant digits or digits after the decimal point,
//             depending on the value of 'mode'
//    decpt:   (out) The offset of the decimal point, relative to the first
//             digit in the return value. This may be outside the string.
//    sign:    (out) The sign of the floating-point number.
//             1 if negative, 0 otherwise.
//    rvc:     (out) A pointer to the character after the last character in
//             the return value.
char *_aves_dtoa(double d, int mode, int ndigits, int *decpt, int *sign, char **rve);
void _aves_freedtoa(char *s);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
// A deleter class, meant to be used with unique_ptr when the result
// comes from _aves_dtoa. It contains no instance data of any kind, and
// calls _aves_freedtoa when invoked with a char*.
class dtoa_deleter
{
public:
	inline dtoa_deleter() { }
	inline dtoa_deleter(dtoa_deleter &other) { }
	inline dtoa_deleter(dtoa_deleter &&other) { }

	inline void operator()(char *s)
	{
		_aves_freedtoa(s);
	}
};
#endif

#endif // AVES__DTOA_CONFIG_H