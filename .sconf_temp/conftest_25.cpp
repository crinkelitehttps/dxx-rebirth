

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


#include "compiler-static_assert.h"
static_assert(true&&true, "global");
struct A
{
	static const bool value = true&&true;
	static_assert(true&&true, "class literal");
	static_assert(A::value, "class static");
	A()
	{
		static_assert(true&&true, "constructor literal");
		static_assert(value, "constructor static");
	}
};
template <typename>
struct B
{
	static const bool value = true&&true;
	static_assert(true&&true, "template class literal");
	static_assert(value, "template class static");
	B(A a)
	{
		static_assert(true&&true, "constructor literal");
		static_assert(value, "constructor self static");
		static_assert(A::value, "constructor static");
		static_assert(a.value, "constructor member");
	}
	template <typename R>
		B(B<R> &&b)
		{
			static_assert(true&&true, "template constructor literal");
			static_assert(value, "template constructor self static");
			static_assert(B<R>::value, "template constructor static");
			static_assert(b.value, "template constructor member");
		}
};
template <typename T>
static void f(B<T> b)
{
	static_assert(true&&true, "template function literal");
	static_assert(B<T>::value, "template function static");
	static_assert(b.value, "template function member");
}
void f(A a);
void f(A a)
{
	static_assert(true&&true, "function literal");
	static_assert(A::value, "function static");
	static_assert(a.value, "function member");
	f(B<long>(B<int>(a)));
}


#undef main	/* avoid -Dmain=SDL_main from libSDL */
int main(int argc,char**argv){(void)argc;(void)argv;
f(A());

;}
