#!/bin/bash
# Build complete musl libc based on OpenEnclave's CMakeLists.txt
# Build everything possible without OE SDK dependencies

# DON'T use set -e, we want to continue on errors
set +e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Find toolchain
if [ -d "/home/abc/arm-toolchain/bin" ]; then
    export PATH="/home/abc/arm-toolchain/bin:$PATH"
fi
export PATH=/home/abc/arm-toolchain/bin:$PATH
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
BUILD_DIR="libc"
MUSLSRC="build_oe_libs/musl/src/src"
MUSL_INCLUDE="build_oe_libs/musl/include"
OELIBC="external/openenclave/libc"

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}Build Complete Musl Libc (OpenEnclave List)${NC}"
echo -e "${GREEN}============================================${NC}"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

GCC_BUILTIN=$( ${CROSS_COMPILE}gcc -print-file-name=include )

COMMON_FLAGS=(
    -fPIC
    -nostdinc
    -isystem "$GCC_BUILTIN"
    -I"../$MUSLSRC/include"
    -I"../$MUSLSRC/internal"
    -I"../$MUSLSRC/../arch/aarch64"
    -I"../$MUSL_INCLUDE"
    -I"../external/openenclave/include"
    -I"../external/openenclave/3rdparty/musl/musl/include"
    -I"../external/openenclave/3rdparty/musl/musl/arch/aarch64"
    -std=c99
    -ffreestanding
    -fvisibility=hidden
    -fno-builtin-memcpy
    -fno-builtin-memset
    -D_XOPEN_SOURCE=700
    -Dhidden=
    -Dweak=
    -Dweak_alias\(old,new\)=
    -w
    -Wno-builtin-macro-redefined
    -Wno-all
    -Wno-error
)

echo -e "${YELLOW}Building musl functions from OpenEnclave CMakeLists.txt...${NC}"

# Complete list from CMakeLists.txt (OP-TEE/TrustZone section)
MUSL_SRCS=(
    # Platform (skip init.c and trace.c - need OE SDK)
    "math/aarch64/sqrt.c"
    "math/aarch64/sqrtf.c"
    "math/exp2l.c"
    "string/memcpy.c"
    "string/memmove.c"
    "string/memset.c"
    # complex - All complex math
    "complex/cabs.c"
    "complex/cabsf.c"
    "complex/cabsl.c"
    "complex/cacos.c"
    "complex/cacosf.c"
    "complex/cacosh.c"
    "complex/cacoshf.c"
    "complex/cacoshl.c"
    "complex/cacosl.c"
    "complex/carg.c"
    "complex/cargf.c"
    "complex/cargl.c"
    "complex/casin.c"
    "complex/casinf.c"
    "complex/casinh.c"
    "complex/casinhf.c"
    "complex/casinhl.c"
    "complex/catan.c"
    "complex/catanf.c"
    "complex/catanh.c"
    "complex/catanhf.c"
    "complex/catanhl.c"
    "complex/catanl.c"
    "complex/ccos.c"
    "complex/ccosf.c"
    "complex/ccosh.c"
    "complex/ccoshf.c"
    "complex/ccoshl.c"
    "complex/ccosl.c"
    "complex/__cexp.c"
    "complex/cexp.c"
    "complex/__cexpf.c"
    "complex/cexpf.c"
    "complex/cexpl.c"
    "complex/cimag.c"
    "complex/cimagf.c"
    "complex/cimagl.c"
    "complex/clog.c"
    "complex/clogf.c"
    "complex/clogl.c"
    "complex/conj.c"
    "complex/conjf.c"
    "complex/conjl.c"
    "complex/cpow.c"
    "complex/cpowf.c"
    "complex/cpowl.c"
    "complex/cproj.c"
    "complex/cprojf.c"
    "complex/cprojl.c"
    "complex/creal.c"
    "complex/crealf.c"
    "complex/creall.c"
    "complex/csin.c"
    "complex/csinf.c"
    "complex/csinh.c"
    "complex/csinhf.c"
    "complex/csinhl.c"
    "complex/csinl.c"
    "complex/csqrt.c"
    "complex/csqrtf.c"
    "complex/csqrtl.c"
    "complex/ctan.c"
    "complex/ctanf.c"
    "complex/ctanh.c"
    "complex/ctanhf.c"
    "complex/ctanhl.c"
    "complex/ctanl.c"
    # conf
    "conf/confstr.c"
    "conf/fpathconf.c"
    "conf/legacy.c"
    "conf/pathconf.c"
    "conf/sysconf.c"
    # ctype - All character classification
    "ctype/__ctype_b_loc.c"
    "ctype/__ctype_get_mb_cur_max.c"
    "ctype/__ctype_tolower_loc.c"
    "ctype/__ctype_toupper_loc.c"
    "ctype/isalnum.c"
    "ctype/isalpha.c"
    "ctype/isascii.c"
    "ctype/isblank.c"
    "ctype/iscntrl.c"
    "ctype/isdigit.c"
    "ctype/isgraph.c"
    "ctype/islower.c"
    "ctype/isprint.c"
    "ctype/ispunct.c"
    "ctype/isspace.c"
    "ctype/isupper.c"
    "ctype/iswalnum.c"
    "ctype/iswalpha.c"
    "ctype/iswblank.c"
    "ctype/iswcntrl.c"
    "ctype/iswctype.c"
    "ctype/iswdigit.c"
    "ctype/iswgraph.c"
    "ctype/iswlower.c"
    "ctype/iswprint.c"
    "ctype/iswpunct.c"
    "ctype/iswspace.c"
    "ctype/iswupper.c"
    "ctype/iswxdigit.c"
    "ctype/isxdigit.c"
    "ctype/toascii.c"
    "ctype/tolower.c"
    "ctype/toupper.c"
    "ctype/towctrans.c"
    "ctype/wcswidth.c"
    "ctype/wctrans.c"
    "ctype/wcwidth.c"
    # env
    "env/clearenv.c"
    "env/__environ.c"
    "env/getenv.c"
    "env/putenv.c"
    "env/setenv.c"
    "env/unsetenv.c"
    # exit
    "exit/assert.c"
    "exit/_Exit.c"
    # fenv
    "fenv/fegetexceptflag.c"
    "fenv/feholdexcept.c"
    "fenv/fesetexceptflag.c"
    "fenv/fesetround.c"
    "fenv/feupdateenv.c"
    "fenv/__flt_rounds.c"
    # internal
    "internal/floatscan.c"
    "internal/intscan.c"
    "internal/libc.c"
    "internal/procfdname.c"
    "internal/shgetc.c"
    # legacy
    "legacy/getpagesize.c"
    # locale
    "locale/bind_textdomain_codeset.c"
    "locale/c_locale.c"
    "locale/duplocale.c"
    "locale/freelocale.c"
    "locale/iconv.c"
    "locale/iconv_close.c"
    "locale/langinfo.c"
    "locale/__lctrans.c"
    "locale/localeconv.c"
    "locale/locale_map.c"
    "locale/__mo_lookup.c"
    "locale/newlocale.c"
    "locale/pleval.c"
    "locale/setlocale.c"
    "locale/strcoll.c"
    "locale/strfmon.c"
    "locale/strxfrm.c"
    "locale/textdomain.c"
    "locale/uselocale.c"
    "locale/wcscoll.c"
    "locale/wcsxfrm.c"
    # math - Complete math library
    "math/__math_divzero.c"
    "math/__math_divzerof.c"
    "math/__math_invalid.c"
    "math/__math_invalidf.c"
    "math/__math_oflow.c"
    "math/__math_oflowf.c"
    "math/__math_uflow.c"
    "math/__math_uflowf.c"
    "math/__math_xflow.c"
    "math/__math_xflowf.c"
    "math/acos.c"
    "math/acosf.c"
    "math/acosh.c"
    "math/acoshf.c"
    "math/acoshl.c"
    "math/asin.c"
    "math/asinf.c"
    "math/asinh.c"
    "math/asinhf.c"
    "math/asinhl.c"
    "math/atan2.c"
    "math/atan2f.c"
    "math/atan2l.c"
    "math/atan.c"
    "math/atanf.c"
    "math/atanh.c"
    "math/atanhf.c"
    "math/atanhl.c"
    "math/atanl.c"
    "math/cbrt.c"
    "math/cbrtf.c"
    "math/cbrtl.c"
    "math/ceil.c"
    "math/ceilf.c"
    "math/ceill.c"
    "math/copysign.c"
    "math/copysignf.c"
    "math/copysignl.c"
    "math/__cos.c"
    "math/cos.c"
    "math/__cosdf.c"
    "math/cosf.c"
    "math/cosh.c"
    "math/coshf.c"
    "math/coshl.c"
    "math/__cosl.c"
    "math/cosl.c"
    "math/erf.c"
    "math/erff.c"
    "math/erfl.c"
    "math/exp_data.c"
    "math/exp10.c"
    "math/exp10f.c"
    "math/exp10l.c"
    "math/exp2.c"
    "math/exp2f.c"
    "math/exp2f_data.c"
    "math/exp.c"
    "math/expf.c"
    "math/expl.c"
    "math/expm1.c"
    "math/expm1f.c"
    "math/expm1l.c"
    "math/__expo2.c"
    "math/__expo2f.c"
    "math/fabs.c"
    "math/fabsf.c"
    "math/fabsl.c"
    "math/fdim.c"
    "math/fdimf.c"
    "math/fdiml.c"
    "math/finite.c"
    "math/finitef.c"
    "math/floor.c"
    "math/floorf.c"
    "math/floorl.c"
    "math/fma.c"
    "math/fmaf.c"
    "math/fmal.c"
    "math/fmax.c"
    "math/fmaxf.c"
    "math/fmaxl.c"
    "math/fmin.c"
    "math/fminf.c"
    "math/fminl.c"
    "math/fmod.c"
    "math/fmodf.c"
    "math/fmodl.c"
    "math/__fpclassify.c"
    "math/__fpclassifyf.c"
    "math/__fpclassifyl.c"
    "math/frexp.c"
    "math/frexpf.c"
    "math/frexpl.c"
    "math/hypot.c"
    "math/hypotf.c"
    "math/hypotl.c"
    "math/ilogb.c"
    "math/ilogbf.c"
    "math/ilogbl.c"
    "math/__invtrigl.c"
    "math/j0.c"
    "math/j0f.c"
    "math/j1.c"
    "math/j1f.c"
    "math/jn.c"
    "math/jnf.c"
    "math/ldexp.c"
    "math/ldexpf.c"
    "math/ldexpl.c"
    "math/lgamma.c"
    "math/lgammaf.c"
    "math/lgammaf_r.c"
    "math/lgammal.c"
    "math/lgamma_r.c"
    "math/llrint.c"
    "math/llrintf.c"
    "math/llrintl.c"
    "math/llround.c"
    "math/llroundf.c"
    "math/llroundl.c"
    "math/log10.c"
    "math/log10f.c"
    "math/log10l.c"
    "math/log1p.c"
    "math/log1pf.c"
    "math/log2.c"
    "math/log2_data.c"
    "math/log2f.c"
    "math/log2f_data.c"
    "math/logb.c"
    "math/logbf.c"
    "math/logbl.c"
    "math/log.c"
    "math/log_data.c"
    "math/logf.c"
    "math/logf_data.c"
    "math/lrint.c"
    "math/lrintf.c"
    "math/lrintl.c"
    "math/lround.c"
    "math/lroundf.c"
    "math/lroundl.c"
    "math/modf.c"
    "math/modff.c"
    "math/modfl.c"
    "math/nan.c"
    "math/nanf.c"
    "math/nanl.c"
    "math/nearbyint.c"
    "math/nearbyintf.c"
    "math/nearbyintl.c"
    "math/nextafter.c"
    "math/nextafterf.c"
    "math/nextafterl.c"
    "math/nexttoward.c"
    "math/nexttowardf.c"
    "math/nexttowardl.c"
    "math/__polevll.c"
    "math/pow.c"
    "math/pow_data.c"
    "math/powf.c"
    "math/powf_data.c"
    "math/powl.c"
    "math/remainder.c"
    "math/remainderf.c"
    "math/remainderl.c"
    "math/__rem_pio2.c"
    "math/__rem_pio2f.c"
    "math/__rem_pio2_large.c"
    "math/__rem_pio2l.c"
    "math/remquo.c"
    "math/remquof.c"
    "math/remquol.c"
    "math/rint.c"
    "math/rintf.c"
    "math/rintl.c"
    "math/round.c"
    "math/roundf.c"
    "math/roundl.c"
    "math/scalb.c"
    "math/scalbf.c"
    "math/scalbln.c"
    "math/scalblnf.c"
    "math/scalblnl.c"
    "math/scalbn.c"
    "math/scalbnf.c"
    "math/scalbnl.c"
    "math/__signbit.c"
    "math/__signbitf.c"
    "math/__signbitl.c"
    "math/signgam.c"
    "math/significand.c"
    "math/significandf.c"
    "math/__sin.c"
    "math/sin.c"
    "math/sincos.c"
    "math/sincosf.c"
    "math/sincosl.c"
    "math/__sindf.c"
    "math/sinf.c"
    "math/sinh.c"
    "math/sinhf.c"
    "math/sinhl.c"
    "math/__sinl.c"
    "math/sinl.c"
    "math/__tan.c"
    "math/tan.c"
    "math/__tandf.c"
    "math/tanf.c"
    "math/tanh.c"
    "math/tanhf.c"
    "math/tanhl.c"
    "math/__tanl.c"
    "math/tanl.c"
    "math/tgamma.c"
    "math/tgammaf.c"
    "math/tgammal.c"
    "math/trunc.c"
    "math/truncf.c"
    "math/truncl.c"
    # misc
    "misc/a64l.c"
    "misc/basename.c"
    "misc/dirname.c"
    "misc/ffs.c"
    "misc/ffsl.c"
    "misc/ffsll.c"
    "misc/get_current_dir_name.c"
    "misc/getauxval.c"
    "misc/getdomainname.c"
    "misc/gethostid.c"
    "misc/getopt.c"
    "misc/getopt_long.c"
    "misc/getsubopt.c"
    "misc/issetugid.c"
    "misc/uname.c"
    # multibyte
    "multibyte/btowc.c"
    "multibyte/c16rtomb.c"
    "multibyte/c32rtomb.c"
    "multibyte/internal.c"
    "multibyte/mblen.c"
    "multibyte/mbrlen.c"
    "multibyte/mbrtoc16.c"
    "multibyte/mbrtoc32.c"
    "multibyte/mbrtowc.c"
    "multibyte/mbsinit.c"
    "multibyte/mbsnrtowcs.c"
    "multibyte/mbsrtowcs.c"
    "multibyte/mbstowcs.c"
    "multibyte/mbtowc.c"
    "multibyte/wcrtomb.c"
    "multibyte/wcsnrtombs.c"
    "multibyte/wcsrtombs.c"
    "multibyte/wcstombs.c"
    "multibyte/wctob.c"
    "multibyte/wctomb.c"
    # prng
    "prng/drand48.c"
    "prng/lcong48.c"
    "prng/lrand48.c"
    "prng/mrand48.c"
    "prng/__rand48_step.c"
    "prng/rand.c"
    "prng/random.c"
    "prng/rand_r.c"
    "prng/__seed48.c"
    "prng/seed48.c"
    "prng/srand48.c"
    # regex
    "regex/fnmatch.c"
    "regex/regerror.c"
    # search
    "search/hsearch.c"
    "search/insque.c"
    "search/lsearch.c"
    "search/tdelete.c"
    "search/tdestroy.c"
    "search/tfind.c"
    "search/tsearch.c"
    "search/twalk.c"
    # stdio - Complete stdio library
    "stdio/asprintf.c"
    "stdio/clearerr.c"
    "stdio/dprintf.c"
    "stdio/ext2.c"
    "stdio/ext.c"
    "stdio/fclose.c"
    "stdio/__fclose_ca.c"
    "stdio/__fdopen.c"
    "stdio/feof.c"
    "stdio/ferror.c"
    "stdio/fflush.c"
    "stdio/fgetc.c"
    "stdio/fgetln.c"
    "stdio/fgetpos.c"
    "stdio/fgets.c"
    "stdio/fgetwc.c"
    "stdio/fgetws.c"
    "stdio/fileno.c"
    "stdio/flockfile.c"
    "stdio/fmemopen.c"
    "stdio/__fmodeflags.c"
    "stdio/fopen.c"
    "stdio/fopencookie.c"
    "stdio/__fopen_rb_ca.c"
    "stdio/fprintf.c"
    "stdio/fputc.c"
    "stdio/fputs.c"
    "stdio/fputwc.c"
    "stdio/fputws.c"
    "stdio/fread.c"
    "stdio/freopen.c"
    "stdio/fscanf.c"
    "stdio/fseek.c"
    "stdio/fsetpos.c"
    "stdio/ftell.c"
    "stdio/ftrylockfile.c"
    "stdio/funlockfile.c"
    "stdio/fwide.c"
    "stdio/fwprintf.c"
    "stdio/fwrite.c"
    "stdio/fwscanf.c"
    "stdio/getc.c"
    "stdio/getchar.c"
    "stdio/getchar_unlocked.c"
    "stdio/getc_unlocked.c"
    "stdio/getdelim.c"
    "stdio/getline.c"
    "stdio/gets.c"
    "stdio/getw.c"
    "stdio/getwc.c"
    "stdio/getwchar.c"
    "stdio/__lockfile.c"
    "stdio/ofl_add.c"
    "stdio/ofl.c"
    "stdio/open_memstream.c"
    "stdio/open_wmemstream.c"
    "stdio/__overflow.c"
    "stdio/perror.c"
    "stdio/printf.c"
    "stdio/putc.c"
    "stdio/putchar.c"
    "stdio/putchar_unlocked.c"
    "stdio/putc_unlocked.c"
    "stdio/puts.c"
    "stdio/putw.c"
    "stdio/putwc.c"
    "stdio/putwchar.c"
    "stdio/remove.c"
    "stdio/rewind.c"
    "stdio/scanf.c"
    "stdio/setbuf.c"
    "stdio/setbuffer.c"
    "stdio/setlinebuf.c"
    "stdio/setvbuf.c"
    "stdio/snprintf.c"
    "stdio/sprintf.c"
    "stdio/sscanf.c"
    "stdio/stderr.c"
    "stdio/stdin.c"
    "stdio/__stdio_close.c"
    "stdio/__stdio_exit.c"
    "stdio/__stdio_read.c"
    "stdio/__stdio_seek.c"
    "stdio/__stdio_write.c"
    "stdio/stdout.c"
    "stdio/__stdout_write.c"
    "stdio/swprintf.c"
    "stdio/swscanf.c"
    "stdio/tempnam.c"
    "stdio/__toread.c"
    "stdio/__towrite.c"
    "stdio/__uflow.c"
    "stdio/ungetc.c"
    "stdio/ungetwc.c"
    "stdio/vasprintf.c"
    "stdio/vdprintf.c"
    "stdio/vfprintf.c"
    "stdio/vfscanf.c"
    "stdio/vfwprintf.c"
    "stdio/vfwscanf.c"
    "stdio/vprintf.c"
    "stdio/vscanf.c"
    "stdio/vsnprintf.c"
    "stdio/vsprintf.c"
    "stdio/vsscanf.c"
    "stdio/vswprintf.c"
    "stdio/vswscanf.c"
    "stdio/vwprintf.c"
    "stdio/vwscanf.c"
    "stdio/wprintf.c"
    "stdio/wscanf.c"
    # stdlib
    "stdlib/abs.c"
    "stdlib/atof.c"
    "stdlib/atoi.c"
    "stdlib/atol.c"
    "stdlib/atoll.c"
    "stdlib/bsearch.c"
    "stdlib/div.c"
    "stdlib/ecvt.c"
    "stdlib/fcvt.c"
    "stdlib/gcvt.c"
    "stdlib/imaxabs.c"
    "stdlib/imaxdiv.c"
    "stdlib/labs.c"
    "stdlib/ldiv.c"
    "stdlib/llabs.c"
    "stdlib/lldiv.c"
    "stdlib/qsort.c"
    "stdlib/strtod.c"
    "stdlib/strtol.c"
    "stdlib/wcstod.c"
    "stdlib/wcstol.c"
    # string - Complete string library
    "string/bcmp.c"
    "string/bcopy.c"
    "string/bzero.c"
    "string/explicit_bzero.c"
    "string/index.c"
    "string/memccpy.c"
    "string/memchr.c"
    "string/memcmp.c"
    "string/memmem.c"
    "string/mempcpy.c"
    "string/memrchr.c"
    "string/rindex.c"
    "string/stpcpy.c"
    "string/stpncpy.c"
    "string/strcasecmp.c"
    "string/strcasestr.c"
    "string/strcat.c"
    "string/strchr.c"
    "string/strchrnul.c"
    "string/strcmp.c"
    "string/strcpy.c"
    "string/strcspn.c"
    "string/strdup.c"
    "string/strerror_r.c"
    "string/strlcat.c"
    "string/strlcpy.c"
    "string/strlen.c"
    "string/strncasecmp.c"
    "string/strncat.c"
    "string/strncmp.c"
    "string/strncpy.c"
    "string/strndup.c"
    "string/strnlen.c"
    "string/strpbrk.c"
    "string/strrchr.c"
    "string/strsep.c"
    "string/strsignal.c"
    "string/strspn.c"
    "string/strstr.c"
    "string/strtok.c"
    "string/strtok_r.c"
    "string/strverscmp.c"
    "string/swab.c"
    "string/wcpcpy.c"
    "string/wcpncpy.c"
    "string/wcscasecmp.c"
    "string/wcscasecmp_l.c"
    "string/wcscat.c"
    "string/wcschr.c"
    "string/wcscmp.c"
    "string/wcscpy.c"
    "string/wcscspn.c"
    "string/wcsdup.c"
    "string/wcslen.c"
    "string/wcsncasecmp.c"
    "string/wcsncasecmp_l.c"
    "string/wcsncat.c"
    "string/wcsncmp.c"
    "string/wcsncpy.c"
    "string/wcsnlen.c"
    "string/wcspbrk.c"
    "string/wcsrchr.c"
    "string/wcsspn.c"
    "string/wcsstr.c"
    "string/wcstok.c"
    "string/wcswcs.c"
    "string/wmemchr.c"
    "string/wmemcmp.c"
    "string/wmemcpy.c"
    "string/wmemmove.c"
    "string/wmemset.c"
    # temp
    "temp/mkdtemp.c"
    "temp/mkostemp.c"
    "temp/mkostemps.c"
    "temp/mkstemp.c"
    "temp/mkstemps.c"
    "temp/mktemp.c"
    "temp/__randname.c"
    # time
    "time/asctime.c"
    "time/asctime_r.c"
    "time/ctime.c"
    "time/ctime_r.c"
    "time/difftime.c"
    "time/ftime.c"
    "time/gmtime.c"
    "time/gmtime_r.c"
    "time/localtime_r.c"
    "time/localtime.c"
    "time/__map_file.c"
    "time/mktime.c"
    "time/__month_to_secs.c"
    "time/__secs_to_tm.c"
    "time/strftime.c"
    "time/strptime.c"
    "time/time.c"
    "time/timegm.c"
    "time/timespec_get.c"
    "time/__tm_to_secs.c"
    "time/__tz.c"
    "time/wcsftime.c"
    "time/__year_to_secs.c"
    # ldso (dynamic linker stubs for OP-TEE - dladdr is weak stub)
    "ldso/dladdr.c"
    # NOTE: pthread from musl needs futex syscalls (oe_SYS_futex_impl, __private_cond_signal, __pthread_mutex_timedlock)
    # OP-TEE is single-threaded - use simple stubs in minimal_stubs.c instead
)

MUSL_OBJS=()
SKIP_COUNT=0
FAIL_COUNT=0
SUCCESS_COUNT=0

for SRC in "${MUSL_SRCS[@]}"; do
    SRC_PATH="../$MUSLSRC/$SRC"
    if [ ! -f "$SRC_PATH" ]; then
        SKIP_COUNT=$((SKIP_COUNT + 1))
        continue
    fi
    
    BASE=$(basename "$SRC" .c)
    SUBDIR=$(dirname "$SRC")
    OBJ="musl_${SUBDIR//\//_}_${BASE}.o"
    
    echo -n "  Building ${SUBDIR}/${BASE}.o... "
    if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" \
        -c "$SRC_PATH" -o "$OBJ" 2>"musl_${SUBDIR//\//_}_${BASE}.log"; then
        # Success if exit code is 0 (even with warnings)
        echo -e "${GREEN}OK${NC}"
        MUSL_OBJS+=("$OBJ")
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        # Only FAIL if actual compilation error (exit code != 0)
        echo -e "${YELLOW}FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

echo ""
echo -e "${GREEN}========== Build Summary ==========${NC}"
echo "  Successfully built: $SUCCESS_COUNT"
echo "  Failed:             $FAIL_COUNT"
echo "  Not found:          $SKIP_COUNT"
echo "  Total objects:      ${#MUSL_OBJS[@]}"
echo ""

if [ ${#MUSL_OBJS[@]} -gt 0 ]; then
    echo -e "${YELLOW}Creating libmusl_complete.a...${NC}"
    ${CROSS_COMPILE}ar rcs libmusl_complete.a "${MUSL_OBJS[@]}"
    ${CROSS_COMPILE}ranlib libmusl_complete.a
    
    SIZE=$(ls -lh libmusl_complete.a | awk '{print $5}')
    echo -e "${GREEN}✓ libmusl_complete.a created (${#MUSL_OBJS[@]} objects, $SIZE)${NC}"
    echo ""
    echo "Library file: $BUILD_DIR/libmusl_complete.a"
else
    echo -e "${RED}✗ No objects built!${NC}"
    exit 1
fi

echo ""
echo -e "${YELLOW}Creating minimal stubs for missing dependencies...${NC}"

# Create minimal_stubs.c with implementations for functions that musl needs
cat > minimal_stubs.c << 'EOF'
// Minimal stubs for missing musl dependencies
// Complete version with libunwind, libgcc, and OpenEnclave stubs

#include <errno.h>
#include <wchar.h>
#include <stdlib.h>
#include <pthread.h>

// ===== Threading stubs (musl stdio needs these) =====
int __lockfile(void* f) { 
    return 0;
}

void __unlockfile(void* f) { 
}

int __lock(volatile int* l) { 
    *l = 1; 
    return 0; 
}

void __unlock(volatile int* l) { 
    *l = 0; 
}

static void* dummy_file_list = 0;

void** __ofl_lock(void) {
    return &dummy_file_list;
}

void __ofl_unlock(void) {
}

// ===== Wide char / multibyte stubs =====
typedef struct _IO_FILE FILE;

int mbtowc(wchar_t* pwc, const char* s, size_t n) {
    if (!s) return 0;
    if (!n || !*s) return 0;
    if (pwc) *pwc = (wchar_t)(unsigned char)*s;
    return 1;
}

wint_t fputwc(wchar_t wc, FILE* stream) {
    return -1;
}

wint_t btowc(int c) {
    return (c >= 0 && c <= 127) ? (wchar_t)c : -1;
}

int fwide(FILE* stream, int mode) {
    return 0;
}

// ===== Error string stub =====
char* strerror(int errnum) {
    static char buf[32];
    switch (errnum) {
        case 0: return "Success";
        case 1: return "Operation not permitted";
        case 2: return "No such file or directory";
        case 12: return "Out of memory";
        case 22: return "Invalid argument";
        case 34: return "Numerical result out of range";
        default: break;
    }
    buf[0] = 'E'; buf[1] = 'r'; buf[2] = 'r'; buf[3] = 'o'; buf[4] = 'r'; buf[5] = ' ';
    int i = 6;
    int n = errnum < 0 ? -errnum : errnum;
    if (errnum < 0) buf[i++] = '-';
    int divisor = 1000;
    int started = 0;
    while (divisor > 0) {
        int digit = n / divisor;
        if (digit > 0 || started || divisor == 1) {
            buf[i++] = '0' + digit;
            started = 1;
        }
        n %= divisor;
        divisor /= 10;
    }
    buf[i] = '\0';
    return buf;
}

// ===== errno location =====
static int __thread_errno = 0;

int* __errno_location(void) {
    return &__thread_errno;
}

int* ___errno_location(void) {
    return &__thread_errno;
}

// ===== pthread stubs for single-threaded OP-TEE =====
// Musl's pthread needs futex syscalls - OP-TEE doesn't have kernel
typedef struct {
    int dummy;
} pthread_mutex_t_stub;

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    return 0;  // Single-threaded - no actual locking needed
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    return 0;  // Single-threaded - no actual unlocking needed
}

int pthread_cond_signal(pthread_cond_t* cond) {
    return 0;  // Single-threaded - no signaling needed
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    return 0;  // Single-threaded - no broadcast needed
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    return 0;  // Single-threaded - no waiting needed
}

// ===== OpenEnclave allocator stubs =====
int oe_allocator_posix_memalign(void** memptr, unsigned long alignment, unsigned long size) {
    void* ptr = malloc(size);
    if (!ptr) return 12;
    *memptr = ptr;
    return 0;
}

void oe_allocator_free(void* ptr) {
    free(ptr);
}

// ===== libunwind signal frame stub =====
int _ULaarch64_is_signal_frame(void* cursor) {
    return 0;
}

// ===== libgcc auxval stub =====
unsigned long __getauxval(unsigned long type) {
    return 0;
}

// ===== OpenEnclave syscall stubs =====
long oe_SYS_close_impl(int fd) {
    return -1;
}

long oe_SYS_lseek_impl(int fd, long offset, int whence) {
    return -1;
}

long oe_SYS_writev_impl(int fd, const void* iov, int iovcnt) {
    return -1;
}

long __syscall_ret(unsigned long r) {
    if (r > -4096UL) {
        __thread_errno = -(long)r;
        return -1;
    }
    return r;
}
EOF

echo "  Created minimal_stubs.c"

# Compile minimal_stubs.c
echo -n "  Compiling minimal_stubs.o... "
if ${CROSS_COMPILE}gcc -c minimal_stubs.c -o minimal_stubs.o \
    -fPIC -nostdinc \
    -isystem "$GCC_BUILTIN" \
    -I"../$MUSL_INCLUDE" \
    -std=c99 -w 2>minimal_stubs.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat minimal_stubs.log
    exit 1
fi

echo ""
echo -e "${YELLOW}Creating complete liboelibc.a (musl + stubs)...${NC}"

# Combine all musl objects + minimal stubs into liboelibc.a
ALL_OBJS=("${MUSL_OBJS[@]}" "minimal_stubs.o")
${CROSS_COMPILE}ar rcs liboelibc.a "${ALL_OBJS[@]}"
${CROSS_COMPILE}ranlib liboelibc.a

SIZE=$(ls -lh liboelibc.a | awk '{print $5}')
echo -e "${GREEN}✓ liboelibc.a created (${#ALL_OBJS[@]} objects, $SIZE)${NC}"
echo ""

# Verify key symbols
echo "Key symbols verification:"
${CROSS_COMPILE}nm liboelibc.a 2>/dev/null | grep -E " T (strtol|strtod|malloc|free|__lockfile|___errno_location)$" | sed 's/^/  /'

echo ""
echo "Library files:"
echo "  - libmusl_complete.a  : ${#MUSL_OBJS[@]} musl objects only"
echo "  - liboelibc.a         : ${#ALL_OBJS[@]} objects (musl + stubs) - USE THIS!"
echo ""
echo -e "${GREEN}Done! Build thừa còn hơn thiếu! 😄${NC}"
echo ""
echo "Next steps:"
echo "  1. ta/Makefile already links liboelibc.a"
echo "  2. Run ./build.sh to build TA"
echo "  3. Deploy and test on Pi 5"
