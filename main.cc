#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <pkeybrd.h>
#include <pvideo.h>
#include <duiwin.h>
#include <cfgtable.h>
#include <common.h>

#define MAXUPDATESKIP	10	/* maximum times to skip updating the screen
								because a command was available. */

Buffer *curbp = 0;		/* Current buffer			*/
View *curvp = 0;			/* Current view				*/
Buffer *bheadp = 0;		/* Buffer listhead			*/
View *vheadp = 0;		/* View listhead			*/

static char lastKeyStartedMacro = 0;

static bool ShutDown = false, ForceDown = false;

/*--------------------------------------------------------------------------
 * This is the general command execution
 * routine. Return true if macro processing
 * should be aborted.
 */
static int ExecuteCommand(unsigned key, unsigned repeat)
{
	BINDING cmd;
	lastKeyStartedMacro = 0;
	cmd = BindKey(key);
	key = cmd.data;
	switch(cmd.type)
	{
		case NOTBOUND:
			MLWrite("This function key is not bound to a function");
			break;
		case INSCHAR:
			while (repeat--)
				if (!InsertOverwriteChar(key))
					return 0;
			break;
		case MACROKEY:
			if (ExecuteMacro(key, repeat))
				lastKeyStartedMacro = 1;
			break;
		case FUNCTIONKEY:
			return functionTable[key].nonr == 0?
				(*(functionTable[key].rept))(repeat):
				(*(functionTable[key].nonr))();
	}
	return 1;
}

/*-----------------------------------------------------------------------------
 */
static void SignalHandler(int sig)
{
	switch (sig)
	{
		case SIGABRT: ForceDown = true;
		case SIGINT:
		case SIGTERM: ShutDown = true;
			break;
	}
}

static int InitSignalHandler(void)
{
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);
	signal(SIGABRT, SignalHandler);
	return 1;
}

static void ExitSignalHandler(void)
{
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGABRT, SIG_DFL);
}

static void DisableSIGINT() { signal(SIGINT, SIG_IGN); }

/*--------------------------------------------------------------------------
 * Local routine to reinit screen and maybe spawn the command interpreter.
 */
static int CommandLine(const char *arg)
{
	ExitKeys();
	ExitScreen();
	ExitSignalHandler();
	ShellOut(arg);
	if (InitKeys() == -1 || InitScreen() == -1)
	{
		puts("Could not reinitialize display after command shell.");
		exit(1);
	}
	InitSignalHandler();
	return 1;
}

/*--------------------------------------------------------------------------
 * Create a subjob with a copy of the command interpreter in it.
 */
int ShellKey(void)
{
	return CommandLine("");
}

/*--------------------------------------------------------------------------
 * Run a one-liner in a subjob.
 */
int CommandKey(void)
{
	Bob cmd;
	return MLReply("Command: ", cmd) == -1? 0: CommandLine(cmd.data());
}

/*--------------------------------------------------------------------------
 */
int main(int argc, char *argv[])
{
	unsigned i, updatesSkipped;
	SetFileExeName(argv[0]);

	if (argc == 1)
	{
		ConfigMain();
		return 0;
	}
	int cfgErrno = ReadConfig()? 0: errno;
	SetActiveModes();
	if (cfgErrno == ENOMEM
			|| !(vheadp = curvp = (View *)calloc(1, sizeof(*vheadp)))
			|| !InitKeys() || !InitScreen()
			|| !InitSignalHandler())
	{
		puts("Could not initialize");
		return 1;
	}
	for (i = 0; --argc; )
		i += WildPathEdit(argv[argc]);
	if (i == 0)
	{
		ExitConfig();
		ExitScreen();
		ExitKeys();
		ExitSignalHandler();
		puts("No valid files to edit.");
		return 1;
	}
	updatesSkipped = 0;
	DisableSIGINT();
	if (cfgErrno && cfgErrno != ENOENT)
		MLWrite("Could not read configuration file: %s",
				cfgErrno == EBADF? "bad version": strerror(cfgErrno));

	for (;;)
	{
		if (ShutDown)
			QuitKey();
		if (macroOut == 0
				&& (updatesSkipped++ > MAXUPDATESKIP || !CommandAvailable()))
		{
			DisplayViews();
			if (!CommandAvailable())
				MLRowCol();
			updatesSkipped = 0;
		}

		/*  now get the keys */
		if ((i = GetCommand()) == NONKEY)
			continue;

		if (i == REFRESHDISPLAYKEY)
		{
			CommandLine(0);
			continue;
		}

		if (!lastKeyStartedMacro)
		{
			if (msgLineFull)
				MLErase();
			curbp->lastSeqCmd = curbp->thisSeqCmd;
			curbp->thisSeqCmd = SEQ_NONE;
		}
		if (!(i & REPT? ExecuteCommand(GetCommand(), i & ~REPT):
				ExecuteCommand(i, 1)))
			macroOut = 0;					/* abort macros on error */
	}
}

/*--------------------------------------------------------------------------
 * Quit command. Confirm if a buffer has been
 * changed and not written out.
 */
int QuitKey(void)
{
	Buffer *oldcurbp, *bp;
	int saveAll = 0;
	oldcurbp = curbp;
	for (bp = bheadp; bp != 0 && !ForceDown; bp = bp->bufp)
		if (bp->flags & B_CHANGED)
			KeyLoop: switch(saveAll? 'A': MLKey("Save \"", bp->fname, "\"? [Yes, No, All, eXit] "))
			{
			case -1: MLErase(); goto AbortQuit;
			case 'A': saveAll = 1; // fall through
			case 'Y': SetBuffer(bp); if (!FileSave()) goto ErrorQuit; break;
			case 'X': goto QuitQuit;
			case 'N': break;
			default: goto KeyLoop;
			}

QuitQuit:
	ExitConfig();
	ExitScreen();
	ExitKeys();
	ExitSignalHandler();
	exit(0);
ErrorQuit:
	MLWrite("Could not save file \"%s\".", bp->fname);
AbortQuit:
	SetBuffer(oldcurbp);
	return 0;
}

/*======================================================================*/
