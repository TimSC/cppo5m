#ifndef CPPO5M_PYSINK_H
#define CPPO5M_PYSINK_H

// Only built when the library is used from a Python extension.
#ifdef PYTHON_AWARE

#ifndef PY_SSIZE_T_CLEAN
#define PY_SSIZE_T_CLEAN
#endif
#include <Python.h>
#include "sink.h"

///Writes to a Python object by calling its write method with bytes. The caller
///must hold the GIL whenever an encoder using this sink is driven.
class PySink : public ByteSink
{
private:
	PyObject *m_PyObj;
	PyObject *m_Write;

public:
	explicit PySink(PyObject *obj);
	virtual ~PySink();
	PySink(const PySink &) = delete;
	PySink &operator=(const PySink &) = delete;

	///Redirects later output to another object, for example a fresh buffer
	///after the previous one has been sent.
	void SetOutput(PyObject *obj);

	using ByteSink::Write;
	///Throws std::runtime_error if the Python call fails; the Python error stays set.
	void Write(const char *data, size_t len) override;
};

#endif //PYTHON_AWARE
#endif //CPPO5M_PYSINK_H
