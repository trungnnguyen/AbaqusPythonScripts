/*
 * winMain.c --
 *
 *	Main entry point for wish and other Tk-based applications.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <locale.h>

/*
 * The following declarations refer to internal Tk routines. These interfaces
 * are available for use, but are not supported.
 */
#ifdef TK_TEST
extern Tcl_PackageInitProc Tktest_Init;
#endif /* TK_TEST */

#if defined(STATIC_BUILD) && TCL_USE_STATIC_PACKAGES
extern Tcl_PackageInitProc Registry_Init;
extern Tcl_PackageInitProc Dde_Init;
extern Tcl_PackageInitProc Dde_SafeInit;
#endif

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		WishPanic(CONST char *format, ...);

static BOOL consoleRequired = TRUE;

/*
 * The following #if block allows you to change the AppInit function by using
 * a #define of TCL_LOCAL_APPINIT instead of rewriting this entire file. The
 * #if checks for that #define and uses Tcl_AppInit if it doesn't exist.
 */

#ifndef TK_LOCAL_APPINIT
#define TK_LOCAL_APPINIT Tcl_AppInit
#endif
extern int TK_LOCAL_APPINIT(Tcl_Interp *interp);

/*
 * The following #if block allows you to change how Tcl finds the startup
 * script, prime the library or encoding paths, fiddle with the argv, etc.,
 * without needing to rewrite Tk_Main()
 */

#ifdef TK_LOCAL_MAIN_HOOK
extern int TK_LOCAL_MAIN_HOOK(int *argc, char ***argv);
#endif

/*
 *----------------------------------------------------------------------
 *
 * WinMain --
 *
 *	Main entry point from Windows.
 *
 * Results:
 *	Returns false if initialization fails, otherwise it never returns.
 *
 * Side effects:
 *	Just about anything, since from here we call arbitrary Tcl code.
 *
 *----------------------------------------------------------------------
 */

int APIENTRY
WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow)
{
    char **argv;
    int argc;
    char *p;

    Tcl_SetPanicProc(WishPanic);

    /*
     * Create the console channels and install them as the standard channels.
     * All I/O will be discarded until Tk_CreateConsoleWindow is called to
     * attach the console to a text widget.
     */

    consoleRequired = TRUE;

    /*
     * Set up the default locale to be standard "C" locale so parsing is
     * performed correctly.
     */

    setlocale(LC_ALL, "C");

    /*
     * Get our args from the c-runtime. Ignore lpszCmdLine.
     */

    argc = __argc;
    argv = __argv;

    /*
     * Forward slashes substituted for backslashes.
     */

    for (p = argv[0]; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }

#ifdef TK_LOCAL_MAIN_HOOK
    TK_LOCAL_MAIN_HOOK(&argc, &argv);
#endif

    Tk_Main(argc, argv, TK_LOCAL_APPINIT);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization. Most
 *	applications, especially those that incorporate additional packages,
 *	will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error message in
 *	the interp's result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(
    Tcl_Interp *interp)		/* Interpreter for application. */
{
#define TK_MAX_WARN_LEN 1024
    WCHAR msgString[TK_MAX_WARN_LEN + 5];

    if (Tcl_Init(interp) == TCL_ERROR) {
	goto error;
    }
    if (Tk_Init(interp) == TCL_ERROR) {
	goto error;
    }
    Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);

    /*
     * Initialize the console only if we are running as an interactive
     * application.
     */

    if (consoleRequired) {
	if (Tk_CreateConsoleWindow(interp) == TCL_ERROR) {
	    goto error;
	}
    }
#if defined(STATIC_BUILD) && TCL_USE_STATIC_PACKAGES
    if (Registry_Init(interp) == TCL_ERROR) {
	goto error;
    }
    Tcl_StaticPackage(interp, "registry", Registry_Init, NULL);

    if (Dde_Init(interp) == TCL_ERROR) {
	goto error;
    }
    Tcl_StaticPackage(interp, "dde", Dde_Init, NULL);
#endif

#ifdef TK_TEST
    if (Tktest_Init(interp) == TCL_ERROR) {
	goto error;
    }
    Tcl_StaticPackage(interp, "Tktest", Tktest_Init, NULL);
#endif /* TK_TEST */

    /*
     * Call the init procedures for included packages. Each call should look
     * like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module. (Dynamically-loadable packages
     * should have the same entry-point name.)
     */

    /*
     * Call Tcl_CreateCommand for application-specific commands, if they
     * weren't already created by the init procedures called above.
     */

    /*
     * Specify a user-specific startup file to invoke if the application is
     * run interactively. Typically the startup file is "~/.apprc" where "app"
     * is the name of the application. If this line is deleted then no user-
     * specific startup file will be run under any conditions.
     */

    Tcl_SetVar(interp, "tcl_rcFileName", "~/wishrc.tcl", TCL_GLOBAL_ONLY);
    return TCL_OK;

error:
    MultiByteToWideChar(CP_UTF8, 0, Tcl_GetStringResult(interp), -1,
	    msgString, TK_MAX_WARN_LEN);
    /*
     * Truncate MessageBox string if it is too long to not overflow the screen
     * and cause possible oversized window error.
     */
	memcpy(msgString + TK_MAX_WARN_LEN, L" ...", 5 * sizeof(WCHAR));
    MessageBeep(MB_ICONEXCLAMATION);
    MessageBoxW(NULL, msgString, L"Error in Wish",
	    MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
    ExitProcess(1);

    /*
     * We won't reach this, but we need the return.
     */

    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * WishPanic --
 *
 *	Display a message and exit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Exits the program.
 *
 *----------------------------------------------------------------------
 */

void
WishPanic(
    CONST char *format, ...)
{
    va_list argList;
    char buf[TK_MAX_WARN_LEN];
    WCHAR msgString[TK_MAX_WARN_LEN + 5];

    va_start(argList, format);
    vsprintf(buf, format, argList);

    MultiByteToWideChar(CP_UTF8, 0, buf, -1, msgString, TK_MAX_WARN_LEN);
    /*
     * Truncate MessageBox string if it is too long to not overflow the screen
     * and cause possible oversized window error.
     */
	memcpy(msgString + TK_MAX_WARN_LEN, L" ...", 5 * sizeof(WCHAR));
    MessageBeep(MB_ICONEXCLAMATION);
    MessageBoxW(NULL, msgString, L"Fatal Error in Wish",
	    MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
#ifdef _MSC_VER
    DebugBreak();
#endif
    ExitProcess(1);
}

#if defined(TK_TEST)
/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Main entry point from the console.
 *
 * Results:
 *	None: Tk_Main never returns here, so this procedure never returns
 *	either.
 *
 * Side effects:
 *	Whatever the applications does.
 *
 *----------------------------------------------------------------------
 */

int
main(
    int argc,
    char **argv)
{
    Tcl_SetPanicProc(WishPanic);

    /*
     * Set up the default locale to be standard "C" locale so parsing is
     * performed correctly.
     */

    setlocale(LC_ALL, "C");

    /*
     * Console emulation widget not required as this entry is from the
     * console subsystem, thus stdin,out,err already have end-points.
     */

    consoleRequired = FALSE;

    Tk_Main(argc, argv, Tcl_AppInit);
    return 0;
}
#endif /* TK_TEST */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
