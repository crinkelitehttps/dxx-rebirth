#ifndef DXXSCONF_H_SEEN
#define DXXSCONF_H_SEEN

// 'g++' => 'g++ (GCC) 5.3.0'
// 'g++ -g -O2 -Wall -Werror=extra -Werror=format=2 -Werror=missing-braces -Werror=missing-include-dirs -Werror=unused -Werror=uninitialized -Werror=undef -Werror=pointer-arith -Werror=cast-qual -Werror=missing-declarations -Werror=redundant-decls -Werror=vla -pthread -funsigned-char -print-prog-name=as' => 'as'
// 'as' => 'GNU assembler (GNU Binutils) 2.26.0.20160302'
// 'g++ -g -O2 -Wall -Werror=extra -Werror=format=2 -Werror=missing-braces -Werror=missing-include-dirs -Werror=unused -Werror=uninitialized -Werror=undef -Werror=pointer-arith -Werror=cast-qual -Werror=missing-declarations -Werror=redundant-decls -Werror=vla -pthread -funsigned-char -print-prog-name=ld' => '/usr/lib/hardening-wrapper/bin/ld'
// '/usr/lib/hardening-wrapper/bin/ld' => 'GNU ld (GNU Binutils) 2.26.0.20160302'
#define _GNU_SOURCE 1
#define MAX_JOYSTICKS 8
#define MAX_AXES_PER_JOYSTICK 128
#define MAX_BUTTONS_PER_JOYSTICK 128
#define MAX_HATS_PER_JOYSTICK 4
#define _GNU_SOURCE 1
#define DXX_HAVE_ATTRIBUTE_ERROR
#define __attribute_error(M) __attribute__((__error__(M)))
#define DXX_HAVE_BUILTIN_BSWAP
#define DXX_HAVE_BUILTIN_BSWAP16
#define DXX_HAVE_BUILTIN_CONSTANT_P
#define dxx_builtin_constant_p(A) __builtin_constant_p(A)
#define DXX_CONSTANT_TRUE(E) (__builtin_constant_p((E)) && (E))
#define likely(A) __builtin_expect(!!(A), 1)
#define unlikely(A) __builtin_expect(!!(A), 0)
#define DXX_HAVE_BUILTIN_OBJECT_SIZE
#define DXX_BEGIN_COMPOUND_STATEMENT 
#define DXX_END_COMPOUND_STATEMENT 
/* 
Declare a function named F and immediately call it.  If gcc's
__attribute__((__error__)) is supported, __attribute_error will expand
to use __attribute__((__error__)) with the explanatory string S, causing
it to be a compilation error if this expression is not optimized out.

Use this macro to implement static assertions that depend on values that
are known to the optimizer, but are not considered "compile time
constant expressions" for the purpose of the static_assert intrinsic.

C++11 deleted functions cannot be used here because the compiler raises
an error for the call before the optimizer has an opportunity to delete
the call via a dead code elimination pass.
 */
#define DXX_ALWAYS_ERROR_FUNCTION(F,S) ( DXX_BEGIN_COMPOUND_STATEMENT {	\
	void F() __attribute_error(S);	\
	F();	\
} DXX_END_COMPOUND_STATEMENT )
#define __attribute_always_inline() __attribute__((__always_inline__))
#define __attribute_alloc_size(A,...) __attribute__((alloc_size(A, ## __VA_ARGS__)))
#define __attribute_cold __attribute__((cold))
#define __attribute_format_arg(A) __attribute__((format_arg(A)))
#define __attribute_format_printf(A,B) __attribute__((format(printf,A,B)))
#define __attribute_malloc() __attribute__((malloc))
#define __attribute_nonnull(...) __attribute__((nonnull __VA_ARGS__))
#define __attribute_noreturn __attribute__((noreturn))
#define __attribute_used __attribute__((used))
#define __attribute_unused __attribute__((unused))
#define __attribute_warn_unused_result __attribute__((warn_unused_result))
#define DXX_HAVE_ATTRIBUTE_WARNING
#define __attribute_warning(M) __attribute__((__warning__(M)))
#define DXX_HAVE_CXX_ARRAY
#define DXX_HAVE_CXX11_STATIC_ASSERT
#define DXX_HAVE_CXX11_TYPE_TRAITS
#define DXX_HAVE_CXX11_RANGE_FOR
#define DXX_HAVE_CONSTEXPR_UNION_CONSTRUCTOR
#define DXX_HAVE_CXX11_BEGIN
#define DXX_HAVE_CXX11_ADDRESSOF
#define DXX_HAVE_CXX14_EXCHANGE
#define DXX_HAVE_CXX14_INTEGER_SEQUENCE
#define DXX_HAVE_CXX14_MAKE_UNIQUE
#define DXX_INHERIT_CONSTRUCTORS(D,B,...) typedef B,##__VA_ARGS__ _dxx_constructor_base_type; \
	using _dxx_constructor_base_type::_dxx_constructor_base_type;
#define DXX_HAVE_CXX11_REF_QUALIFIER
#define DXX_HAVE_STRCASECMP

#if defined(DXX_BUILD_DESCENT_I) || defined(DXX_BUILD_DESCENT_II)
#if defined(DXX_BUILD_DESCENT_I)
#define dsx d1x
#else
#define dsx d2x
#endif
namespace dsx {	/* Force type mismatch on attempted nesting */
	class dcx;	/* dcx declared inside dsx */
	class dsx;	/* dsx declared inside dsx */
}
using namespace dsx;
#else
class dsx;	/* dsx declared in common-only code */
#endif

namespace dcx {	/* Force type mismatch on attempted nesting */
	class dcx;	/* dcx declared inside dcx */
	class dsx;	/* dsx declared inside dcx */
}
using namespace dcx;
namespace {
	class dcx;	/* dcx declared inside anonymous */
	class dsx;	/* dsx declared inside anonymous */
}

#endif /* DXXSCONF_H_SEEN */
