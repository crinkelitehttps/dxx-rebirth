

/* This test is always false.  Use a non-trivial condition to
 * discourage external text scanners from recognizing that the block
 * is never compiled.
 */
#if 1 < -1
-2381b4fe
// 'g++' => 'g++ (GCC) 5.3.0'
// 'g++ -g -O2 -Wall -Werror=extra -Werror=format=2 -Werror=missing-braces -Werror=missing-include-dirs -Werror=unused -Werror=uninitialized -Werror=undef -Werror=pointer-arith -Werror=cast-qual -Werror=missing-declarations -Werror=redundant-decls -Werror=vla -pthread -funsigned-char -print-prog-name=as' => 'as'
// 'as' => 'GNU assembler (GNU Binutils) 2.26.0.20160302'
// 'g++ -g -O2 -Wall -Werror=extra -Werror=format=2 -Werror=missing-braces -Werror=missing-include-dirs -Werror=unused -Werror=uninitialized -Werror=undef -Werror=pointer-arith -Werror=cast-qual -Werror=missing-declarations -Werror=redundant-decls -Werror=vla -pthread -funsigned-char -print-prog-name=ld' => '/usr/lib/hardening-wrapper/bin/ld'
// '/usr/lib/hardening-wrapper/bin/ld' => 'GNU ld (GNU Binutils) 2.26.0.20160302'

#endif



/* Test for bug where clang + libc++ + constructor inheritance causes a
 * compilation failure when returning nullptr.
 *
 * Works: gcc
 * Works: clang + gcc libstdc++
 * Works: old clang + old libc++ (cutoff date unknown).
 * Works: new clang + new libc++ + unique_ptr<T>
 * Fails: new clang + new libc++ + unique_ptr<T[]> (v3.6.0 confirmed broken).

memory:2676:32: error: no type named 'type' in 'std::__1::enable_if<false, std::__1::unique_ptr<int [], std::__1::default_delete<int []> >::__nat>'; 'enable_if' cannot be used to disable this declaration
            typename enable_if<__same_or_less_cv_qualified<_Pp, pointer>::value, __nat>::type = __nat()) _NOEXCEPT
                               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.sconf_temp/conftest_43.cpp:26:11: note: in instantiation of member function 'std::__1::unique_ptr<int [], std::__1::default_delete<int []> >::unique_ptr' requested here
        using B::B;
                 ^
.sconf_temp/conftest_43.cpp:30:2: note: while substituting deduced template arguments into function template 'I' [with _Pp = I]
        return nullptr;
        ^
 */
#include <memory>
class I : std::unique_ptr<int[]>
{
public:
	typedef std::unique_ptr<int[]> B;
	using B::B;
};
I a();
I a()
{
	return nullptr;
}

#define DXX_INHERIT_CONSTRUCTORS(D,B,...) typedef B,##__VA_ARGS__ _dxx_constructor_base_type; \
	using _dxx_constructor_base_type::_dxx_constructor_base_type;
struct A {
	A(int){}
};
struct B:A {
DXX_INHERIT_CONSTRUCTORS(B,A);
};


#undef main	/* avoid -Dmain=SDL_main from libSDL */
int main(int argc,char**argv){(void)argc;(void)argv;
B(0)

;}
