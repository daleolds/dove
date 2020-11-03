#if defined(DOVE_FOR_GNU)

#include <assert.h>
#include <pvideo.h>
#include <pkeybrd.h>

unsigned TTYGetKeys(unsigned maxKeys, unsigned *keys, unsigned millisecondsToWait);

#define MAX_UNGETS  30

static unsigned keybuf[MAX_UNGETS], *keybp = keybuf;

/* -------------------------------------------------------------------
 * for gnu the keyboard and video routines are combined for terminfo
 */
int InitKeys(void) { return InitVideo(); }
void ExitKeys(void) { ExitVideo(); }

int KeyHit(unsigned millisecondsToWait)
{
	unsigned key;
	if (keybp == keybuf && TTYGetKeys(1, &key, millisecondsToWait))
		UngetKey(key);
	return keybp - keybuf;
}

unsigned GetKey(void)
{
	while (!KeyHit(100))
		;
	assert(keybp > keybuf && keybp < keybuf + MAX_UNGETS);
	return *--keybp;
}

void UngetKey(unsigned key)
{
	assert(keybp >= keybuf && keybp < keybuf + MAX_UNGETS);
	if (keybp < keybuf + MAX_UNGETS)
		*keybp++ = key;
}

//===========================================================================
#endif
