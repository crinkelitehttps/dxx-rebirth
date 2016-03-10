

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

#include <SDL.h>

#undef main	/* avoid -Dmain=SDL_main from libSDL */
int main(int argc,char**argv){(void)argc;(void)argv;

	SDL_RWops *ops = reinterpret_cast<SDL_RWops *>(argv);
#if MAX_JOYSTICKS
#define DXX_SDL_INIT_JOYSTICK	SDL_INIT_JOYSTICK |
#else
#define DXX_SDL_INIT_JOYSTICK
#endif
	SDL_Init(DXX_SDL_INIT_JOYSTICK SDL_INIT_CDROM | SDL_INIT_VIDEO | SDL_INIT_AUDIO);
#if MAX_JOYSTICKS
	auto n = SDL_NumJoysticks();
	(void)n;
#endif
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	SDL_FreeRW(ops);
	SDL_Quit();


;}
