

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


struct test_constexpr {};
static constexpr test_constexpr get_test_constexpr(){return {};}

#include <cstddef>
std::nullptr_t test_nullptr1 = nullptr;
int *test_nullptr2 = nullptr;

struct test_explicit_operator_bool {
	explicit operator bool();
};

using test_template_aliases_typedef = int;
template <typename>
struct test_template_aliases_struct;
template <typename T>
using test_template_aliases_alias = test_template_aliases_struct<T>;

auto test_trailing_function_return_type()->int;

struct test_class_scope_static_constexpr_assignment_instance {
};
struct test_class_scope_static_constexpr_assignment_container {
	static constexpr test_class_scope_static_constexpr_assignment_instance a = {};
};

struct test_braced_base_class_initialization_base {
	int a;
};
struct test_braced_base_class_initialization_derived : test_braced_base_class_initialization_base {
	test_braced_base_class_initialization_derived(int e) : test_braced_base_class_initialization_base{e} {}
};

#include <unordered_map>


#undef main	/* avoid -Dmain=SDL_main from libSDL */
int main(int argc,char**argv){(void)argc;(void)argv;
{
	get_test_constexpr();
}
{
	test_nullptr2 = test_nullptr1;
}
{
	test_template_aliases_struct<int> *a = nullptr;
	test_template_aliases_alias<int> *b = a;
	test_template_aliases_typedef *c = nullptr;
	(void)b;
	(void)c;
}
{
	std::unordered_map<int,int> m;
	m.emplace(0, 0);
}


;}
