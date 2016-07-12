/*
 *  Scilab ( http://www.scilab.org/ ) - This file is part of Scilab
 *  Copyright (C) 2016 - Leonardo José Consoni
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

/**
 ** \file JupyterMessageRead.h
 ** \brief Define JupyterKernel message reading C wrapper function.
 */

#ifndef __JUPYTER_READ_H__
#define __JUPYTER_READ_H__
/*--------------------------------------------------------------------------*/
#include "dynlib_jupyter.h"
/*--------------------------------------------------------------------------*/
/** \brief C wrapper of JupyterKernel::GetInputString(void) function 
 ** \return command string to be processed */
JUPYTER_IMPEXP char *JupyterMessageRead(void);

/*--------------------------------------------------------------------------*/
#endif /* __JUPYTER_READ_H__ */

