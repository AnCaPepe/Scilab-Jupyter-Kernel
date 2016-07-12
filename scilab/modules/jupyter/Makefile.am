#
# Scilab ( http://www.scilab.org/ ) - This file is part of Scilab
# Copyright (C) INRIA   - 2006-2008 - Sylvestre Ledru
# Copyright (C) INRIA   - 2007 - Vincent COUVERT
# Copyright (C) DIGITEO - 2009 - Sylvestre Ledru
# Copyright (C) DIGITEO - 2009 - Pierre MARECHAL
# Copyright (C) DIGITEO - 2011 - Allan CORNET
# Copyright (C) DIGITEO - 2011 - Antoine ELIAS
#
# Copyright (C) 2012 - 2016 - Scilab Enterprises
#
# This file is hereby licensed under the terms of the GNU GPL v2.0,
# pursuant to article 5.3.4 of the CeCILL v.2.1.
# This file was originally licensed under the terms of the CeCILL v2.1,
# and continues to be available under such terms.
# For more information, see the COPYING file which you should have received
# along with this program.
#


### SOURCES ###
JUPYTER_CPP_SOURCES = \
    src/cpp/InitializeJupyter.cpp \
    src/cpp/jsoncpp.cpp \
    src/cpp/JupyterKernel.cpp \
    src/cpp/JupyterKernelConnection.cpp \
    src/cpp/JupyterMessage.cpp \
    src/cpp/JupyterMessageHash.cpp \
    src/cpp/JupyterMessagePrint.cpp \
    src/cpp/JupyterMessageRead.cpp \
    src/cpp/uuid.cpp

# Includes need for the compilation
libscijupyter_la_CPPFLAGS = \
    -I$(top_srcdir)/modules/api_scilab/includes \
    -I$(srcdir)/includes/ \
    -I$(srcdir)/src/cpp/ \
    -I$(top_srcdir)/modules/ast/includes/ast/ \
    -I$(top_srcdir)/modules/ast/includes/analysis/ \
    -I$(top_srcdir)/modules/ast/includes/exps/ \
    -I$(top_srcdir)/modules/ast/includes/operations/ \
    -I$(top_srcdir)/modules/ast/includes/parse/ \
    -I$(top_srcdir)/modules/ast/includes/symbol/ \
    -I$(top_srcdir)/modules/ast/includes/system_env/ \
    -I$(top_srcdir)/modules/ast/includes/types/ \
    -I$(top_srcdir)/modules/history_manager/includes/ \
    -I$(top_srcdir)/modules/ui_data/includes/ \
    -I$(top_srcdir)/modules/threads/includes \
    -I$(top_srcdir)/modules/completion/includes \
    -I$(top_srcdir)/modules/console/includes \
    -I$(top_srcdir)/modules/output_stream/includes \
    -I$(top_srcdir)/modules/string/includes \
    -I$(top_srcdir)/modules/fileio/includes \
    -I$(top_srcdir)/modules/localization/includes \
    -I$(top_srcdir)/modules/commons/src/jni \
    -I$(top_srcdir)/modules/dynamic_link/includes \
    $(AM_CPPFLAGS)

# Name of the library
pkglib_LTLIBRARIES = libscijupyter.la
#noinst_LTLIBRARIES = libscijupyter-algo.la

# All the sources needed by libscijupyter.la
libscijupyter_la_SOURCES = $(JUPYTER_CPP_SOURCES)

libscijupyter_la_LDFLAGS = $(AM_LDFLAGS) $(JUPYTER_LIBS)
libscijupyter_la_LIBADD = $(JUPYTER_LIBS)



#### Target ######
modulename=jupyter

#### jupyter : include files ####
libscijupyter_la_includedir=$(pkgincludedir)
libscijupyter_la_include_HEADERS = \
includes/dynlib_jupyter.h \
includes/InitializeJupyter.h \
includes/JupyterMessagePrint.h \
includes/JupyterMessageRead.h


include $(top_srcdir)/Makefile.incl.am
