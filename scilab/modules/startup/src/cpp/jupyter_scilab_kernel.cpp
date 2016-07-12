/*
 *  Scilab ( http://www.scilab.org/ ) - This file is part of Scilab
 *  Copyright (C) 2006-2008 - DIGITEO - Bruno JOFRET
 *  Copyright (C) 2013 - Scilab Enterprises - Antoine ELIAS
 *
 * Copyright (C) 2012 - 2016 - Scilab Enterprises
 *
 * This file is hereby licensed under the terms of the GNU GPL v2.0,
 * pursuant to article 5.3.4 of the CeCILL v.2.1.
 * This file was originally licensed under the terms of the CeCILL v2.1,
 * and continues to be available under such terms.
 * For more information, see the COPYING file which you should have received
 * along with this program.
 *
 */


#ifdef _MSC_VER
#pragma comment(lib,"../../../../../bin/libintl.lib")
#endif

#include <cstdio>
#include <iostream>
#include <string.h>
#include <setjmp.h>

extern "C"
{
#ifdef __APPLE__
#include "initMacOSXEnv.h"
#endif
#include "InitScilab.h"
#include "scilabRead.h"
#include "InitializeJupyter.h"
#include "JupyterMessageRead.h"
#include "JupyterMessagePrint.h"
#include "configvariable_interface.h"
#include "version.h"
#include "sci_malloc.h"
#include "lasterror.h"

#ifdef _MSC_VER
#include "FilesAssociations.h"
#include "PATH_MAX.h"
    jmp_buf ScilabJmpEnv;
#else
    extern jmp_buf ScilabJmpEnv;
#endif
#include "isatty.hxx"
}

#include "configvariable.hxx"
#include "exit_status.hxx"
#include "scilabWrite.hxx"

#define INTERACTIVE     -1

// extern "C"
// {
// #ifdef ENABLE_MPI
// #include "initMPI.h"
// #endif
// }

/*
** Usage
**
** Display usage : options available
*/
static void usage(void)
{
    std::cerr << "Usage: jupyter-scilab-kernel <config_file> <options>" << std::endl;
    std::cerr << "      --help           : Display this help." << std::endl;
    std::cerr << "      --version        : Display version number." << std::endl;
    std::cerr << "Developer Trace arguments:" << std::endl;
    std::cerr << "      --parse-trace    : Display bison state machine evolution." << std::endl;
    std::cerr << "      --AST-trace      : Display ASCII-art AST to be human readable." << std::endl;
    std::cerr << "      --pretty-print   : Display pretty-printed code, standard Scilab syntax." << std::endl;
    std::cerr << " " << std::endl;
    std::cerr << "Developer Timer arguments:" << std::endl;
    std::cerr << "      --AST-timed      : Time each AST node." << std::endl;
    std::cerr << "      --timed          : Time global execution." << std::endl;
    std::cerr << " " << std::endl;
    std::cerr << "Developer Debug arguments:" << std::endl;
    std::cerr << "      --no-exec        : Only do Lexing/parsing do not execute instructions." << std::endl;
    std::cerr << "      --context-dump   : Display context status." << std::endl;
    std::cerr << "      --exec-verbose   : Display command before running it." << std::endl;
    std::cerr << "      --timeout delay  : Kill the Scilab process after a delay." << std::endl;
}

/*
** Get Options
**
*/
static int get_option(const int argc, char *argv[], ScilabEngineInfo* _pSEI)
{
    int i = 0;

#ifdef DEBUG
    std::cerr << "-*- Getting Options -*-" << std::endl;
#endif

    for (i = 1; i < argc; ++i)
    {
        if (!strcmp("--parse-trace", argv[i]))
        {
            _pSEI->iParseTrace = 1;
        }
        else if (!strcmp("--pretty-print", argv[i]))
        {
            _pSEI->iPrintAst = 1;
        }
        else if (!strcmp("--help", argv[i]))
        {
            usage();
            exit(WELL_DONE);
        }
        else if (!strcmp("--AST-trace", argv[i]))
        {
            _pSEI->iDumpAst = 1;
        }
        else if (!strcmp("--no-exec", argv[i]))
        {
            _pSEI->iExecAst = 0;
        }
        else if (!strcmp("--context-dump", argv[i]))
        {
            _pSEI->iDumpStack = 1;
        }
        else if (!strcmp("--timed", argv[i]))
        {
            _pSEI->iTimed = 1;
            ConfigVariable::setTimed(true);
        }
        else if (!strcmp("--serialize", argv[i]))
        {
            _pSEI->iSerialize = 1;
            ConfigVariable::setSerialize(true);
        }
        else if (!strcmp("--AST-timed", argv[i]))
        {
            std::cout << "Timed execution" << std::endl;
            _pSEI->iAstTimed = 1;
        }
        else if (!strcmp("--parse-file", argv[i]))
        {
            i++;
            if (argc >= i)
            {
                _pSEI->pstParseFile = argv[i];
            }
        }
        else if (!strcmp("--version", argv[i]))
        {
            disp_scilab_version();
            exit(WELL_DONE);
        }
        else if (!strcmp("--exec-verbose", argv[i]))
        {
            _pSEI->iExecVerbose = 1;
        }
        else if (!strcmp("--timeout", argv[i]))
        {
            i++;
            if (argc > i)
            {
                char* timeout = argv[i];

                char* str_end = NULL;
                int iTimeoutDelay = strtol(timeout, &str_end, 0);

                int modifier;
                switch (*str_end)
                {
                    case 'd':
                        modifier = 86400;
                        break;
                    case 'h':
                        modifier = 3600;
                        break;
                    case 'm':
                        modifier = 60;
                        break;
                    case 's':
                    case '\0': // no modifiers
                        modifier = 1;
                        break;
                    default:
                        std::cerr << "Invalid timeout delay unit: s (for seconds, default), m (for minutes), h (for hours), d (for days) are supported" << std::endl;
                        exit(EXIT_FAILURE);
                        break;
                }

                _pSEI->iTimeoutDelay = iTimeoutDelay * modifier;
            }
            else
            {
                std::cerr << "Unspecified timeout delay" << std::endl;
                exit(EXIT_FAILURE);
            }

        }
        else if (!strcmp("--keepconsole", argv[i]))
        {
            _pSEI->iKeepConsole = 1;
        }
    }

    ConfigVariable::setCommandLineArgs(argc, argv);
    return 0;
}

/*
** -*- MAIN -*-
*/
//#if defined(_WIN32) && !defined(WITHOUT_GUI)
//int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow)
//#else
int main(int argc, char *argv[])
//#endif
{
    int iRet = 0;

// #ifdef ENABLE_MPI
//     initScilabMPI();
// #endif

    ScilabEngineInfo* pSEI = InitScilabEngineInfo();

    /* Building Scilab-cli-bin. We won't ever had the gui nor the jvm */
    pSEI->iConsoleMode = 1;
    pSEI->iNoJvm = 1;
    setScilabMode(SCILAB_NWNI); //setScilabMode(SCILAB_NW);

    get_option(argc, argv, pSEI);

    setScilabInputMethod( &JupyterMessageRead );
    setScilabOutputMethod( &JupyterMessagePrint );
// #if defined(__APPLE__)
//     if (pSEI->iNoJvm == 0)
//     {
//         return initMacOSXEnv(pSEI);
//     }
// #endif // !defined(__APPLE__)

// #if defined(__APPLE__)
//     return initMacOSXEnv(pSEI);
// #endif // !defined(__APPLE__)

    int val = setjmp(ScilabJmpEnv);
    if (!val)
    {
        InitializeJupyter( ( argc > 1 ) ? argv[ 1 ] : NULL );
        
        iRet = StartScilabEngine(pSEI);
        if (iRet == 0)
        {
            iRet = RunScilabEngine(pSEI);
        }

        StopScilabEngine(pSEI);
        FREE(pSEI);
        
        TerminateJupyter();
        
        return iRet;
    }
    else
    {
        // We probably had a segfault so print error
        std::wcerr << getLastErrorMessage() << std::endl;
        return val;
    }
}
