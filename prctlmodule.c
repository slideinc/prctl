/*
 * Copyright (c) 2005-2010 Slide, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of the author nor the names of other
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Python.h>
#include <sys/prctl.h>
#include <sys/errno.h>

static char module_doc[] =
"This module provides access to the Linux prctl system call\n\
";

static char prctl_doc[] =
"prctl(option, [value]) -> result\n\n\
When just the option value is provided return the current setting. Set\n\
the option to a new value if the optional value parameter is supplied.\n\n\
Valid Options:\n\n\
  PDEATHSIG: Receive signal (as defined by value) on parent exit\n\
  DUMPABLE:  current->mm->dumpable\n\
  UNALIGN:   Unaligned access control bits (if meaningful) \n\
  KEEPCAPS:  Whether or not to drop capabilities on setuid() away\n\
             from uid 0\n\
  FPEMU:     Floating-point emulation control bits (if meaningful)\n\
  FPEXC:     Floating-point exception mode (if meaningful)\n\
  TIMING:    Whether we use statistical process timing or accurate\n\
             timestamp\n\
  NAME:      Process name\n\
  ENDIAN:    Process endianess\n\
";

static PyObject *ErrorObject;

struct table_entry {
	char *name;
	char *desc;
	int   get;
	int   set;
};

#define PR_PDEATHSIG 0
#define PR_DUMPABLE  1
#define PR_UNALIGN   2
#define PR_KEEPCAPS  3
#define PR_FPEMU     4
#define PR_FPEXC     5
#define PR_TIMING    6
#define PR_NAME      7

#define MIN_ENTRY PR_PDEATHSIG
#define MAX_ENTRY PR_NAME

#ifdef PR_GET_ENDIAN
#define PR_ENDIAN 8
#undef  MAX_ENTRY
#define MAX_ENTRY PR_ENDIAN
#endif

#define MAX_LEN 1024 /* well more then maximum kernel size (TASK_COMM_LEN) */

static struct table_entry _option_table[] = {
	{"PDEATHSIG", NULL, PR_GET_PDEATHSIG, PR_SET_PDEATHSIG},
	{"DUMPABLE",  NULL, PR_GET_DUMPABLE,  PR_SET_DUMPABLE},
	{"UNALIGN",   NULL, PR_GET_UNALIGN,   PR_SET_UNALIGN},
	{"KEEPCAPS",  NULL, PR_GET_KEEPCAPS,  PR_SET_KEEPCAPS},
	{"FPEMU",     NULL, PR_GET_FPEMU,     PR_SET_FPEMU},
	{"FPEXC",     NULL, PR_GET_FPEXC,     PR_SET_FPEXC},
	{"TIMING",    NULL, PR_GET_TIMING,    PR_SET_TIMING},
	{"NAME",      NULL, PR_GET_NAME,      PR_SET_NAME},
#ifdef PR_ENDIAN
	{"ENDIAN",    NULL, PR_GET_ENDIAN,    PR_SET_ENDIAN},
#endif
	{NULL, NULL, 0, 0}
};

static PyObject *_set_prctl(int option, PyObject *value)
{
	unsigned long arg;
	int result;

	switch (option) {
	case PR_NAME:
		if (PyString_Check(value)) {
			arg = (unsigned long)PyString_AS_STRING(value);
			break;
		}

		if (PyUnicode_Check(value)) {
			value = PyUnicode_AsUTF8String(value);
			arg = (unsigned long)PyString_AS_STRING(value);

			Py_DECREF(value);
			break;
		}

		PyErr_SetString(PyExc_TypeError, "option/value type error");
		return NULL;
	default:
		if (PyInt_Check(value)) {
			arg = PyInt_AS_LONG(value);
			break;
		}

		if (PyLong_Check(value)) {
			arg = PyLong_AsLongLong(value);
			break;
		}

		PyErr_SetString(PyExc_TypeError, "option/value type error");
		return NULL;
	}


	result = prctl(_option_table[option].set, arg);
	if (result < 0) {
		PyErr_SetFromErrno(ErrorObject);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *_get_prctl(int option)
{
	PyObject *output;
	char arg[MAX_LEN];
	int  result;

	memset(arg, 0, MAX_LEN);

	result = prctl(_option_table[option].get, arg);
	if (result < 0) {
		PyErr_SetFromErrno(ErrorObject);
		return NULL;
	}
	
	switch (option) {
	case PR_PDEATHSIG:
		output = PyInt_FromLong(*(int *)arg);
		break;
	case PR_NAME:
		output = PyString_FromString(arg);
		break;
	default:
		output = PyInt_FromLong(result);
		break;
	}

	return output;
}

static PyObject *py_prctl(PyObject *self, PyObject *args)
{
	PyObject *value = NULL;
	int option;

	if (!PyArg_ParseTuple(args, "i|O", &option, &value))
		return NULL;

	if (option < MIN_ENTRY || option > MAX_ENTRY) {
		PyErr_SetString(PyExc_ValueError, "invalid option");
		return NULL;
	}

	if (value)
		return _set_prctl(option, value);
	else
		return _get_prctl(option);
}


static PyMethodDef _prctl_methods[] = {
	{"prctl", py_prctl, METH_VARARGS, prctl_doc},
	{NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initprctl(void)
{
	PyObject *module;
	PyObject *dict;
	int i = 0;

	module = Py_InitModule3("prctl", _prctl_methods, module_doc);
	if (!module)
		return;

	dict = PyModule_GetDict(module);
	if (!dict)
		return;

	ErrorObject = PyErr_NewException("prctl.PrctlError", NULL, NULL);
	PyDict_SetItemString(dict, "PrctlError", ErrorObject);

	while (_option_table[i].name) {
		PyModule_AddIntConstant(module, _option_table[i].name, i);
		i++;
	}
}

/*
 * Local Variables:
 * c-file-style: "linux"
 * indent-tabs-mode: t
 * End:
 */
