

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

#include <physfs.h>

#undef main	/* avoid -Dmain=SDL_main from libSDL */
int main(int argc,char**argv){(void)argc;(void)argv;

	PHYSFS_File *f;
	char b[1] = {0};
	PHYSFS_init("");
	f = PHYSFS_openWrite("a");
	PHYSFS_sint64 w = PHYSFS_write(f, b, 1, 1);
	(void)w;
	PHYSFS_close(f);
	f = PHYSFS_openRead("a");
	PHYSFS_sint64 r = PHYSFS_read(f, b, 1, 1);
	(void)r;
	PHYSFS_close(f);


;}
