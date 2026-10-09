#include "pysink.h"

#ifdef PYTHON_AWARE
#include <stdexcept>

PySink::PySink(PyObject *obj) : m_PyObj(nullptr), m_Write(nullptr)
{
	this->SetOutput(obj);
}

PySink::~PySink()
{
	Py_XDECREF(m_Write);
	Py_XDECREF(m_PyObj);
}

void PySink::SetOutput(PyObject *obj)
{
	if(obj == nullptr)
		throw std::invalid_argument("Python output object is null");
	PyObject *write = PyObject_GetAttrString(obj, "write");
	if(write == nullptr)
		throw std::invalid_argument("Python output object has no write method");
	Py_INCREF(obj);
	Py_XDECREF(m_Write);
	Py_XDECREF(m_PyObj);
	m_PyObj = obj;
	m_Write = write;
}

void PySink::Write(const char *data, size_t len)
{
	PyObject *ret = PyObject_CallFunction(m_Write, "y#", data, (Py_ssize_t)len);
	if(ret == nullptr)
		throw std::runtime_error("Python write call failed");
	Py_DECREF(ret);
}

#endif //PYTHON_AWARE
