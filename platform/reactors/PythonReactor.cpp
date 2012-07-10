// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2011 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Pion is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Pion is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
// more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Pion.  If not, see <http://www.gnu.org/licenses/>.
//

// NOTE: According to API docs, Python.h must be #include'd FIRST
#include <Python.h>
#include <frameobject.h>
#include <cstring>
#include "structmember.h"
#include "datetime.h"
#include <sstream>
#include <fstream>
#include <boost/filesystem/operations.hpp>
#include <pion/platform/ConfigManager.hpp>
#include "PythonReactor.hpp"

// for compatibility with Python < 2.5
#if PY_VERSION_HEX < 0x02050000
	typedef int Py_ssize_t;
	typedef inquiry lenfunc;
	#define PY_SSIZE_T_MAX INT_MAX
	#define PY_SSIZE_T_MIN INT_MIN
#endif

using namespace std;
using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


// Define the various classes and callback functions for the pion Python module
// see http://docs.python.org/extending/newtypes.html

static bool
Python_getTermRef(const Vocabulary& v, PyObject *obj, Vocabulary::TermRef& term_ref)
{
	term_ref = Vocabulary::UNDEFINED_TERM_REF;
	
	if (PyInt_Check(obj) || PyLong_Check(obj)) {
		term_ref = PyLong_AsUnsignedLong(obj);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF) {
			PyErr_SetString(PyExc_KeyError, "undefined term reference");
		} else if (term_ref > v.size()) {
			term_ref = Vocabulary::UNDEFINED_TERM_REF;
			PyErr_SetString(PyExc_KeyError, "out-of-range term reference");
		}
	} else if (PyString_Check(obj)) {
		const char *term_str = PyString_AsString(obj);
		term_ref = v.findTerm(term_str);
		if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
			(void)PyErr_Format(PyExc_KeyError, "term '%s' not found", term_str);
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
	}
	
	return (term_ref != Vocabulary::UNDEFINED_TERM_REF);
}

// forward declaration so that Reactor_event and Event_copy can use it
static PyObject *Event_create(PyObject *reactor_ptr, const EventPtr& event_ptr, bool is_unique);


// pion.reactor python class

typedef struct {
	PyObject_HEAD
	PyObject *		dict;
	PythonReactor *	__this;
} PythonReactorObject;

static void
Reactor_dealloc(PythonReactorObject* self)
{
	Py_XDECREF(self->dict);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
Reactor_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PythonReactorObject *self;
	self = (PythonReactorObject *)type->tp_alloc(type, 0);
	if (self != NULL) {
		self->dict = PyDict_New();
		if (self->dict == NULL) {
			Py_DECREF(self);
			return NULL;
		}
		self->__this = NULL;
	}
	return (PyObject *)self;
}

static PyObject*
Reactor_event(PythonReactorObject *self, PyObject *args)
{
	// get argument and make sure it's a string
	PyObject *term_id_obj;
	// Note: event_ptr is a borrowed reference -- do not decrement ref count
	if (! PyArg_ParseTuple(args, "O:reactor.event", &term_id_obj)) {
		PyErr_SetString(PyExc_TypeError, "missing required parameter");
		return NULL;
	}
	
	// get event type
	Vocabulary::TermRef event_type;
	PythonReactor *ptr = self->__this;
	PION_ASSERT(ptr);
	if (! Python_getTermRef(ptr->getVocabulary(), term_id_obj, event_type))
		return NULL;
	
	// create a new empty event
	EventFactory f;
	EventPtr new_ptr;
	f.create(new_ptr, event_type);

	return Event_create((PyObject*)self, new_ptr, true);
}

static PyObject*
Reactor_deliver(PythonReactorObject *self, PyObject *args)
{
	// Note: event_ptr is a borrowed reference -- do not decrement ref count
	PyObject *event_ptr;
	if (! PyArg_ParseTuple(args, "O:reactor.deliver", &event_ptr)) {
		PyErr_SetString(PyExc_TypeError, "missing required parameter");
		return NULL;
	}

	// Note: Reactor::deliverToConnections() checks that type == pion.event

	// deliver the event to other reactors
	PythonReactor *ptr = self->__this;
	PION_ASSERT(ptr);
	if (! ptr->deliverToConnections(event_ptr))
		return NULL;

	// return "none" (OK)
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject*
Reactor_getsession(PythonReactorObject *self, PyObject *args)
{
	// Note: event_ptr is a borrowed reference -- do not decrement ref count
	PyObject *event_ptr;
	if (! PyArg_ParseTuple(args, "O:reactor.getsession", &event_ptr)) {
		PyErr_SetString(PyExc_TypeError, "missing required parameter");
		return NULL;
	}

	// Note: Reactor::getSession() checks that type == pion.event

	PythonReactor *ptr = self->__this;
	PION_ASSERT(ptr);
	return ptr->getSession(event_ptr);
}

static PyObject*
Reactor_getterm(PythonReactorObject *self, PyObject *args)
{
	// get argument and make sure it's a string
	PyObject *term_id_obj;
	// Note: event_ptr is a borrowed reference -- do not decrement ref count
	if (! PyArg_ParseTuple(args, "O:reactor.getterm", &term_id_obj)) {
		PyErr_SetString(PyExc_TypeError, "missing required parameter");
		return NULL;
	}
	char *term_id = PyString_AsString(term_id_obj);
	if (term_id == NULL || *term_id == '\0') {
		PyErr_SetString(PyExc_TypeError, "parameter must be a string");
		return NULL;
	}
	PION_ASSERT(self->__this);
	return PyLong_FromUnsignedLong(self->__this->getVocabulary().findTerm(term_id));
}

static PyObject*
Reactor_GetAttr(PyObject *obj, PyObject *attr_name)
{
	PyObject *retval = PyObject_GenericGetAttr(obj, attr_name);

	if (retval == NULL) {
		PyErr_Clear();
		PythonReactorObject *self = (PythonReactorObject*) obj;
		const char *attr_str = PyString_AsString(attr_name);
		PION_ASSERT(self->__this);
		if (strcmp(attr_str, "id") == 0) {
			retval = PyString_FromString(self->__this->getId().c_str());
		} else if (strcmp(attr_str, "name") == 0) {
			retval = PyString_FromString(self->__this->getName().c_str());
		} else {
			// note: PyDict_GetItem doesn't set error
			retval = PyDict_GetItem(self->dict, attr_name);
			if (retval == NULL) {
				(void)PyErr_Format(PyExc_AttributeError, "'pion.reactor' object has no attribute '%s'", attr_str);
			} else {
				Py_INCREF(retval);
			}
		}
	}
	
	return retval;
}

static int
Reactor_SetAttr(PyObject *obj, PyObject *attr_name, PyObject *value)
{
	int retval = -1;
	char *attr_str = PyString_AsString(attr_name);
	PythonReactorObject *self = (PythonReactorObject*) obj;

	if (strcmp(attr_str, "id") == 0 || strcmp(attr_str, "name") == 0) {
		(void)PyErr_Format(PyExc_AttributeError, "Read-only attribute: %s", attr_str);
		retval = -1;
	} else if (value == NULL) {
		retval = PyDict_DelItem(self->dict, attr_name);
	} else {
		retval = PyDict_SetItem(self->dict, attr_name, value);
	}
	
	return retval;
}

static PyMethodDef Reactor_methods[] = {
	{(char*)"event", (PyCFunction)Reactor_event, METH_VARARGS,
	(char*)"Constructs a new pion.event object for the given type."},
	{(char*)"deliver", (PyCFunction)Reactor_deliver, METH_VARARGS,
	(char*)"Delivers an event to the reactor's output connections."},
	{(char*)"getterm", (PyCFunction)Reactor_getterm, METH_VARARGS,
	(char*)"Returns a numeric term reference for the given identifer."},
	{(char*)"getsession", (PyCFunction)Reactor_getsession, METH_VARARGS,
	(char*)"Returns a unique object for the event's session."},
    {NULL}  /* Sentinel */
};

static PyMemberDef Reactor_members[] = {
    {NULL}  /* Sentinel */
};

static PyTypeObject PythonReactorType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /*ob_size*/
	"pion.reactor",            /*tp_name*/
	sizeof(PythonReactorObject), /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Reactor_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	Reactor_GetAttr,           /*tp_getattro*/
	Reactor_SetAttr,           /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
	"pion reactor objects",    /* tp_doc */
	0,		                   /* tp_traverse */
	0,		                   /* tp_clear */
	0,		                   /* tp_richcompare */
	0,		                   /* tp_weaklistoffset */
	0,		                   /* tp_iter */
	0,		                   /* tp_iternext */
	Reactor_methods,           /* tp_methods */
	Reactor_members,           /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,                         /* tp_init */
	0,                         /* tp_alloc */
	Reactor_new,               /* tp_new */
};

static PythonReactorObject *
Reactor_create(PythonReactor *this_ptr)
{
	PythonReactorObject *self;
	self = (PythonReactorObject *) PythonReactorType.tp_alloc(&PythonReactorType, 0);
	if (self != NULL) {
		self->dict = PyDict_New();
		self->__this = this_ptr;
	}
	return self;
}


// pion.event python class

typedef struct {
	PyObject_HEAD
	bool					is_unique;
	EventPtr				event_ptr;
	PythonReactorObject	*	reactor_ptr;
} PythonEventObject;

static void
Event_dealloc(PythonEventObject* self)
{
	self->event_ptr.reset();
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
Event_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PythonEventObject *self;
	self = (PythonEventObject *)type->tp_alloc(type, 0);
	if (self != NULL) {
		self->is_unique = false;
		self->reactor_ptr = NULL;
	}
	return (PyObject *)self;
}

static const Vocabulary&
Event_getVocabulary(PythonEventObject *self)
{
	PION_ASSERT(self->reactor_ptr);
	PION_ASSERT(self->reactor_ptr->__this);
	return self->reactor_ptr->__this->getVocabulary();
}

static bool
Event_getTermRef(PythonEventObject *self, PyObject *obj, Vocabulary::TermRef& term_ref)
{
	return Python_getTermRef(Event_getVocabulary(self), obj, term_ref);
}

static int
Event_init(PythonEventObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *reactor_ptr=NULL;
	PyObject *type=NULL;
	static char *kwlist[] = {(char*)"reactor", (char*)"type", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "OO:pion.event", kwlist, &reactor_ptr, &type))
		return -1; 

	EventFactory f;
	self->reactor_ptr = (PythonReactorObject*) reactor_ptr;
	PION_ASSERT(self->reactor_ptr);
	Event::EventType event_type;
	if (! Event_getTermRef(self, type, event_type))
		return -1;

	f.create(self->event_ptr, event_type);
	self->is_unique = true;

	return 0;
}

static int
Event_print(PyObject *obj, FILE *fp, int flags)
{
	PythonEventObject *self = (PythonEventObject*) obj;

	if (self->event_ptr.get() != NULL) {
		PION_ASSERT(self->reactor_ptr);
		const Event& e = *(self->event_ptr);
		std::string str;

		fprintf(fp, "\npion.event (type=%s)\n===========================================================\n", Event_getVocabulary(self)[e.getType()].term_id.c_str());

		for (Event::ConstIterator it = e.begin(); it != e.end(); ++it) {
			const Vocabulary::Term& term = Event_getVocabulary(self)[it->term_ref];
			Event::write(str, it->value, term);
			fprintf(fp, "--- %s = %s\n", term.term_id.c_str(), str.c_str());
		}
	} else {
		fprintf(fp, "pion.event (empty)\n");
	}

	return 0;
}

static PyObject* 
Event_getValue(const Event::ParameterValue& value, Vocabulary::DataType term_type)
{
	// needed for python date time API functions
	PyDateTime_IMPORT; 

	// create return object based upon term type
	PyObject *retval = NULL;
	switch(term_type) {
	case Vocabulary::TYPE_NULL:
	case Vocabulary::TYPE_OBJECT:
		// this shouldn't happen in practice, but if it does this is the most logical conversion
		retval = PyInt_FromLong(1);
		break;
	case Vocabulary::TYPE_INT8:
	case Vocabulary::TYPE_INT16:
	case Vocabulary::TYPE_INT32:
		retval = PyInt_FromLong(boost::get<boost::int32_t>(value));
		break;
	case Vocabulary::TYPE_UINT8:
	case Vocabulary::TYPE_UINT16:
		retval = PyInt_FromLong(boost::get<boost::uint32_t>(value));
		break;
	case Vocabulary::TYPE_UINT32:
		retval = PyLong_FromUnsignedLong(boost::get<boost::uint32_t>(value));
		break;
	case Vocabulary::TYPE_INT64:
		retval = PyLong_FromLongLong(boost::get<boost::int64_t>(value));
		break;
	case Vocabulary::TYPE_UINT64:
		retval = PyLong_FromUnsignedLongLong(boost::get<boost::uint64_t>(value));
		break;
	case Vocabulary::TYPE_FLOAT:
		retval = PyFloat_FromDouble(boost::get<float>(value));
		break;
	case Vocabulary::TYPE_DOUBLE:
		retval = PyFloat_FromDouble(boost::get<double>(value));
		break;
	case Vocabulary::TYPE_LONG_DOUBLE:
		retval = PyFloat_FromDouble(boost::get<long double>(value));
		break;
	case Vocabulary::TYPE_SHORT_STRING:
	case Vocabulary::TYPE_STRING:
	case Vocabulary::TYPE_LONG_STRING:
	case Vocabulary::TYPE_CHAR:
	case Vocabulary::TYPE_BLOB:
	case Vocabulary::TYPE_ZBLOB:
		{
		const Event::BlobType& b = boost::get<const Event::BlobType&>(value);
		retval = PyString_FromStringAndSize(b.get(), b.size());
		break;
		}
	case Vocabulary::TYPE_DATE_TIME:
		{
		const PionDateTime& d = boost::get<const PionDateTime&>(value);
		retval = PyDateTime_FromDateAndTime(d.date().year(),
			d.date().month(), d.date().day(),
			d.time_of_day().hours(), d.time_of_day().minutes(),
			d.time_of_day().seconds(),
			d.time_of_day().total_microseconds() % 1000000UL);
		break;
		}
	case Vocabulary::TYPE_DATE:
		{
		const PionDateTime& d = boost::get<const PionDateTime&>(value);
		retval = PyDate_FromDate(d.date().year(), d.date().month(),
			d.date().day());
		break;
		}
	case Vocabulary::TYPE_TIME:
		{
		const PionDateTime& d = boost::get<const PionDateTime&>(value);
		retval = PyTime_FromTime(d.time_of_day().hours(),
			d.time_of_day().minutes(), d.time_of_day().seconds(),
			d.time_of_day().total_microseconds() % 1000000UL);
		break;
		}
	}

	return retval;
}

static PyObject*
Event_getBase(PythonEventObject *self, PyObject *term, PyObject *default_value)
{
	PyObject *retval = NULL;
	Vocabulary::TermRef term_ref;
	if (! Event_getTermRef(self, term, term_ref))
		return NULL;
	
	PION_ASSERT(self->event_ptr.get());

	// get pointer to value in event
	const Event::ParameterValue *param_ptr = self->event_ptr->getPointer(term_ref);

	if (param_ptr == NULL) {

		if (default_value) {
			Py_INCREF(default_value);
			retval = default_value;
		} else {
			Py_INCREF(Py_None);
			retval = Py_None;
		}

	} else {

		retval = Event_getValue(*param_ptr, Event_getVocabulary(self)[term_ref].term_type);
		if (retval == NULL) {
			PyErr_SetString(PyExc_RuntimeError, "event parameter conversion failed");
		}
	} 
	
	return retval;
}

static PyObject* 
Event_getMap(PythonEventObject *self, PyObject *term)
{
	return Event_getBase(self, term, NULL);
}

static PyObject* 
Event_getFunc(PythonEventObject *self, PyObject *args)
{
	// get term and default parameters
	PyObject *term = NULL;
	PyObject *default_value = NULL;

	// Note: obj is a borrowed reference -- do not decrement ref count
	if (! PyArg_ParseTuple(args, "O|O:event.get", &term, &default_value)) {
		PyErr_SetString(PyExc_TypeError, "missing required parameter");
		return NULL;
	}
	
	return Event_getBase(self, term, default_value);
}

static PyObject* 
Event_getlist(PythonEventObject *self, PyObject *args)
{
	// get term parameter
	PyObject *term = NULL;
	PyObject *default_value = NULL;
	// Note: obj is a borrowed reference -- do not decrement ref count
	if (! PyArg_ParseTuple(args, "O|O:event.getlist", &term, &default_value)) {
		PyErr_SetString(PyExc_TypeError, "missing required parameter");
		return NULL;
	}
	
	PyObject *retval = NULL;
	Vocabulary::TermRef term_ref;
	if (! Event_getTermRef(self, term, term_ref))
		return NULL;
	
	// get range of all values for given term
	Event::ValuesRange range = self->event_ptr->equal_range(term_ref);
	
	if (range.first == range.second) {

		if (default_value) {
			Py_INCREF(default_value);
			retval = default_value;
		} else {
			retval = PyList_New(0);
		}

	} else {

		const Vocabulary::DataType term_type = Event_getVocabulary(self)[term_ref].term_type;
		retval = PyList_New(0);

		for (Event::ConstIterator it = range.first; it != range.second; ++it) {
			PyObject *param = Event_getValue(it->value, term_type);
			if (param == NULL) {
				PyErr_SetString(PyExc_RuntimeError, "event parameter conversion failed");
				Py_DECREF(retval);
				return NULL;
			}
			PyList_Append(retval, param);
		}
	} 
	
	return retval;
}

static Py_ssize_t
Event_conversion_error(const std::string& term_id, const char *msg)
{
	std::string error_msg(msg);
	error_msg += " for ";
	error_msg += term_id;
	PyErr_SetString(PyExc_TypeError, error_msg.c_str());
	return -1;
}

static Py_ssize_t
Event_setTerm(PythonEventObject *self, Vocabulary::TermRef term_ref, PyObject *value)
{
	// needed for python date time API functions
	PyDateTime_IMPORT; 

	const Vocabulary::Term& t = Event_getVocabulary(self)[term_ref];
	Event& e = *self->event_ptr;

	// set value while converting type
	switch( t.term_type ) {
	case Vocabulary::TYPE_NULL:
	case Vocabulary::TYPE_OBJECT:
		// although not really supported, this is the most logical conversion
		e.setInt(term_ref, 1);
		break;
	case Vocabulary::TYPE_INT8:
	case Vocabulary::TYPE_INT16:
	case Vocabulary::TYPE_INT32:
		if (PyLong_Check(value))
			e.setInt(term_ref, PyLong_AsLong(value) );
		else if (PyInt_Check(value))
			e.setInt(term_ref, PyInt_AsLong(value) );
		else
			return Event_conversion_error(t.term_id, "int or long required");
		break;
	case Vocabulary::TYPE_UINT8:
	case Vocabulary::TYPE_UINT16:
		if (PyLong_Check(value))
			e.setUInt(term_ref, PyLong_AsUnsignedLong(value) );
		else if (PyInt_Check(value))
			e.setUInt(term_ref, PyInt_AsLong(value) );
		else
			return Event_conversion_error(t.term_id, "int or long required");
		break;
	case Vocabulary::TYPE_UINT32:
		if (PyLong_Check(value))
			e.setUInt(term_ref, PyLong_AsUnsignedLong(value) );
		else if (PyInt_Check(value))
			e.setUInt(term_ref, PyInt_AsUnsignedLongMask(value) );
		else
			return Event_conversion_error(t.term_id, "int or long required");
		break;
	case Vocabulary::TYPE_INT64:
		if (PyLong_Check(value))
			e.setBigInt(term_ref, PyLong_AsLongLong(value) );
		else if (PyInt_Check(value))
			e.setBigInt(term_ref, PyInt_AsLong(value) );
		else
			return Event_conversion_error(t.term_id, "int or long required");
		break;
	case Vocabulary::TYPE_UINT64:
		if (PyLong_Check(value))
			e.setUBigInt(term_ref, PyLong_AsUnsignedLongLong(value) );
		else if (PyInt_Check(value))
			e.setUBigInt(term_ref, PyInt_AsUnsignedLongLongMask(value) );
		else
			return Event_conversion_error(t.term_id, "int or long required");
		break;
	case Vocabulary::TYPE_FLOAT:
		if (PyFloat_Check(value))
			e.setFloat(term_ref, PyFloat_AsDouble(value) );
		else if (PyInt_Check(value))
			e.setFloat(term_ref, PyInt_AsLong(value) );
		else
			return Event_conversion_error(t.term_id, "int or float required");
		break;
	case Vocabulary::TYPE_DOUBLE:
		if (PyFloat_Check(value))
			e.setDouble(term_ref, PyFloat_AsDouble(value) );
		else if (PyInt_Check(value))
			e.setDouble(term_ref, PyInt_AsLong(value) );
		else
			return Event_conversion_error(t.term_id, "int or float required");
		break;
	case Vocabulary::TYPE_LONG_DOUBLE:
		if (PyFloat_Check(value))
			e.setLongDouble(term_ref, PyFloat_AsDouble(value) );
		else if (PyInt_Check(value))
			e.setLongDouble(term_ref, PyInt_AsLong(value) );
		else
			return Event_conversion_error(t.term_id, "int or float required");
		break;
	case Vocabulary::TYPE_SHORT_STRING:
	case Vocabulary::TYPE_STRING:
	case Vocabulary::TYPE_LONG_STRING:
	case Vocabulary::TYPE_CHAR:
	case Vocabulary::TYPE_BLOB:
	case Vocabulary::TYPE_ZBLOB:
		if (PyString_Check(value)) {
			char *buf = PyString_AsString(value);
			Py_ssize_t len = PyString_Size(value);
			e.setString(term_ref, buf, len);
		} else {
			return Event_conversion_error(t.term_id, "str required");
		}
		break;
	case Vocabulary::TYPE_DATE_TIME:
		{
		if (! PyDateTime_Check(value))
			return Event_conversion_error(t.term_id, "datetime required");
		boost::gregorian::date date(PyDateTime_GET_YEAR(value),
			PyDateTime_GET_MONTH(value), PyDateTime_GET_DAY(value));
		boost::posix_time::time_duration time(PyDateTime_DATE_GET_HOUR(value),
			PyDateTime_DATE_GET_MINUTE(value), PyDateTime_DATE_GET_SECOND(value),
			PythonReactor::boost_msec_to_fsec(PyDateTime_DATE_GET_MICROSECOND(value)) );
		PionDateTime d(date, time);
		e.setDateTime(term_ref, d);
		break;
		}
	case Vocabulary::TYPE_DATE:
		{
		if (PyDateTime_Check(value)) {
			boost::gregorian::date date(PyDateTime_GET_YEAR(value),
				PyDateTime_GET_MONTH(value), PyDateTime_GET_DAY(value));
			boost::posix_time::time_duration time(PyDateTime_DATE_GET_HOUR(value),
				PyDateTime_DATE_GET_MINUTE(value), PyDateTime_DATE_GET_SECOND(value),
				PythonReactor::boost_msec_to_fsec(PyDateTime_DATE_GET_MICROSECOND(value)) );
			PionDateTime d(date, time);
			e.setDateTime(term_ref, d);
		} else if (PyDate_Check(value)) {
			boost::gregorian::date date(PyDateTime_GET_YEAR(value),
				PyDateTime_GET_MONTH(value), PyDateTime_GET_DAY(value));
			PionDateTime d(date);
			e.setDateTime(term_ref, d);
		} else {
			return Event_conversion_error(t.term_id, "date or datetime required");
		}
		break;
		}
	case Vocabulary::TYPE_TIME:
		{
		if (PyDateTime_Check(value)) {
			boost::gregorian::date date(PyDateTime_GET_YEAR(value),
				PyDateTime_GET_MONTH(value), PyDateTime_GET_DAY(value));
			boost::posix_time::time_duration time(PyDateTime_DATE_GET_HOUR(value),
				PyDateTime_DATE_GET_MINUTE(value), PyDateTime_DATE_GET_SECOND(value),
				PythonReactor::boost_msec_to_fsec(PyDateTime_DATE_GET_MICROSECOND(value)) );
			PionDateTime d(date, time);
			e.setDateTime(term_ref, d);
		} else if (PyTime_Check(value)) {
			boost::gregorian::date date(1970, 1, 1);
			boost::posix_time::time_duration time(PyDateTime_TIME_GET_HOUR(value),
				PyDateTime_TIME_GET_MINUTE(value), PyDateTime_TIME_GET_SECOND(value),
				PythonReactor::boost_msec_to_fsec(PyDateTime_TIME_GET_MICROSECOND(value)) );
			PionDateTime d(date, time);
			e.setDateTime(term_ref, d);
		} else {
			return Event_conversion_error(t.term_id, "time or datetime required");
		}
		break;
		}
	}

	return 0;
}

static Py_ssize_t
Event_setBase(PythonEventObject *self, PyObject *term, PyObject *obj, bool clear_first)
{
	PION_ASSERT(self->event_ptr.get());

	// check if we need to duplicate the event before modifying it
	if (! self->is_unique) {

		// TODO: add EventPtr::clone() function
		//self->event_ptr = self->event_ptr.clone();

		EventFactory f;
		EventPtr old_ptr(self->event_ptr);
		f.create(self->event_ptr, old_ptr->getType());
		*self->event_ptr += *old_ptr;

		self->is_unique = true;
	}

	// determine which term is being set
	Vocabulary::TermRef term_ref;
	if (! Event_getTermRef(self, term, term_ref))
		return -1;
	
	// clear any existing values first?
	if (clear_first)
		self->event_ptr->clear(term_ref);
	
	// check if we have a single value or sequence of values
	Py_ssize_t retval = 0;
	if (PySequence_Check(obj) && !PyString_Check(obj)) {
		Py_ssize_t size = PySequence_Size(obj);
		if (size > 0) {
			for (Py_ssize_t n = 0; n < size; ++n) {
				retval = Event_setTerm(self, term_ref, PySequence_GetItem(obj, n));
				if (retval != 0)
					break;
			}
		}
	} else {
		retval = Event_setTerm(self, term_ref, obj);
	}
	
	return retval;
}

static Py_ssize_t
Event_setMap(PythonEventObject *self, PyObject *key, PyObject *value)
{
	return Event_setBase(self, key, value, true);
}

static PyObject *
Event_setFunc(PythonEventObject *self, PyObject *args)
{
	// get parameters
	PyObject *term;
	PyObject *value;
	// Note: obj is a borrowed reference -- do not decrement ref count
	if (! PyArg_ParseTuple(args, "OO:event.set", &term, &value)) {
		PyErr_SetString(PyExc_TypeError, "missing required parameter");
		return NULL;
	}

	PyObject *retval = NULL;
	Py_ssize_t result = Event_setBase(self, term, value, false);
	if (result == 0) {
		Py_INCREF(Py_None);
		retval = Py_None;
	}

	return retval;
}

static PyObject*
Event_clear(PythonEventObject *self, PyObject *args)
{
	// get parameters
	PyObject *term;
	// Note: obj is a borrowed reference -- do not decrement ref count
	if (! PyArg_ParseTuple(args, "|O:event.clear", &term)) {
		PyErr_SetString(PyExc_TypeError, "error parsing arguments");
		return NULL;
	}
	
	if (term) {
		Vocabulary::TermRef term_ref;
		if (! Event_getTermRef(self, term, term_ref))
			return NULL;
		if (self->event_ptr.get())
			self->event_ptr->clear(term_ref);
	} else {
		if (self->event_ptr.get())
			self->event_ptr->clear();
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject*
Event_empty(PythonEventObject *self)
{
	PyObject *retval = NULL;

	if (self->event_ptr.get() == NULL) {
		retval = Py_True;
	} else {
		retval = (self->event_ptr->empty() ? Py_True : Py_False);
	}

	Py_INCREF(retval);
	return retval;
}

static PyObject*
Event_copy(PythonEventObject *self)
{
	// TODO: add EventPtr::clone() function
	//self->event_ptr = self->event_ptr.clone();

	EventFactory f;
	EventPtr new_ptr;
	f.create(new_ptr, self->event_ptr->getType());
	*new_ptr += *self->event_ptr;
	
	return Event_create((PyObject*)self->reactor_ptr, new_ptr, true);
}

static PyObject*
Event_has_key(PythonEventObject *self, PyObject *args)
{
	// get parameters
	PyObject *term;
	// Note: obj is a borrowed reference -- do not decrement ref count
	if (! PyArg_ParseTuple(args, "O:event.has_key", &term)) {
		PyErr_SetString(PyExc_TypeError, "error parsing arguments");
		return NULL;
	}
	
	// get term reference
	Vocabulary::TermRef term_ref;
	if (! Event_getTermRef(self, term, term_ref))
		return NULL;

	PyObject *retval = Py_False;
	if (self->event_ptr.get()) {
		if (self->event_ptr->isDefined(term_ref))
			retval = Py_True;
	}

	Py_INCREF(retval);
	return retval;
}

static PyObject*
Event_getReactor(PythonEventObject *self, void *closure)
{
	PION_ASSERT(self->reactor_ptr);
	Py_INCREF(self->reactor_ptr);
	return (PyObject*) self->reactor_ptr;
}

static PyObject *
Event_getType(PythonEventObject *self, void *closure)
{
	Event::EventType event_type = Vocabulary::UNDEFINED_TERM_REF;
	if (self->event_ptr.get() != NULL)
		event_type = self->event_ptr->getType();
	return PyLong_FromUnsignedLong(event_type);
}

static PyObject *
Event_getTypeString(PythonEventObject *self, void *closure)
{
	Event::EventType event_type = Vocabulary::UNDEFINED_TERM_REF;
	if (self->event_ptr.get() != NULL)
		event_type = self->event_ptr->getType();
	return PyString_FromString(Event_getVocabulary(self)[event_type].term_id.c_str());
}

static Py_ssize_t
Event_size(PyObject *obj)
{
	//TODO: not implemented b/c not supported by native Event type
	PyErr_SetString(PyExc_NotImplementedError, "len(pion.event) not implemented");
	return -1;
}

static PyMappingMethods Event_map_methods = {
	(lenfunc)Event_size,
	(binaryfunc)Event_getMap,
	(objobjargproc)Event_setMap,
};

static PyMethodDef Event_methods[] = {
	{(char*)"getlist", (PyCFunction)Event_getlist, METH_VARARGS,
	(char*)"Returns a list of all values for a given term."},
	{(char*)"get", (PyCFunction)Event_getFunc, METH_VARARGS,
	(char*)"Returns the value of an Event parameter."},
	{(char*)"set", (PyCFunction)Event_setFunc, METH_VARARGS,
	(char*)"Sets the value of an Event parameter."},
	{(char*)"clear", (PyCFunction)Event_clear, METH_VARARGS,
	(char*)"Clears all items from the Event."},
	{(char*)"empty", (PyCFunction)Event_empty, METH_NOARGS,
	(char*)"Checks to see if the Event contains zero items."},
	{(char*)"copy", (PyCFunction)Event_copy, METH_NOARGS,
	(char*)"Creates and returns a unique copy of the event."},
	{(char*)"has_key", (PyCFunction)Event_has_key, METH_VARARGS,
	(char*)"Returns True if the event has one or more values for a given term."},
    {NULL}  /* Sentinel */
};

static PyMemberDef Event_members[] = {
    {NULL}  /* Sentinel */
};

static PyGetSetDef Event_getseters[] = {
    {(char*)"type", 
     (getter)Event_getType, NULL,
     (char*)"numeric identifier for the type of event",
     NULL},
    {(char*)"typestr", 
     (getter)Event_getTypeString, NULL,
     (char*)"string identifier for the type of event",
     NULL},
    {(char*)"reactor", 
     (getter)Event_getReactor, NULL,
     (char*)"reactor associated with this event",
     NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject PythonEventType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /*ob_size*/
	"pion.event",              /*tp_name*/
	sizeof(PythonEventObject), /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Event_dealloc, /*tp_dealloc*/
	Event_print,               /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	&Event_map_methods,        /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /*tp_flags*/
	"pion event objects",      /* tp_doc */
	0,		                   /* tp_traverse */
	0,		                   /* tp_clear */
	0,		                   /* tp_richcompare */
	0,		                   /* tp_weaklistoffset */
	0,		                   /* tp_iter */
	0,		                   /* tp_iternext */
	Event_methods,             /* tp_methods */
	Event_members,             /* tp_members */
	Event_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)Event_init,      /* tp_init */
	0,                         /* tp_alloc */
	Event_new,                 /* tp_new */
};

static PyObject *
Event_create(PyObject *reactor_ptr, const EventPtr& event_ptr, bool is_unique)
{
	PythonEventObject *self;
	self = (PythonEventObject *) PythonEventType.tp_alloc(&PythonEventType, 0);
	if (self != NULL) {
		self->is_unique = is_unique;
		self->event_ptr = event_ptr;
		self->reactor_ptr = (PythonReactorObject*) reactor_ptr;
	}
	return (PyObject*) self;
}


// pion.session python class

typedef struct {
	PyObject_HEAD
	PyObject *id;
	PyObject *dict;
} PythonSessionObject;

static void
Session_dealloc(PythonSessionObject* self)
{
	Py_XDECREF(self->id);
	Py_XDECREF(self->dict);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
Session_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PythonSessionObject *self = (PythonSessionObject *)type->tp_alloc(type, 0);
	if (self != NULL) {
		self->id = PyString_FromString("");
		if (self->id == NULL) {
			Py_DECREF(self);
			return NULL;
		}
		self->dict = PyDict_New();
		if (self->dict == NULL) {
			Py_DECREF(self->id);
			Py_DECREF(self);
			return NULL;
		}
	}
	return (PyObject *)self;
}

static int
Session_init(PythonSessionObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {(char*)"session_id", NULL};
	PyObject *session_id=NULL;
	PyObject *tmp=NULL;

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "O:pion.session", kwlist, &session_id))
		return -1;

	if (session_id) {
		tmp = self->id;
		Py_INCREF(session_id);
		self->id = session_id;
		Py_XDECREF(tmp);
	}
	
	return 0;
}

static PyObject*
Session_GetAttr(PyObject *obj, PyObject *attr_name)
{
	PyObject *retval = PyObject_GenericGetAttr(obj, attr_name);

	if (retval == NULL) {
		PyErr_Clear();
		PythonSessionObject *self = (PythonSessionObject*) obj;
		char *attr_str = PyString_AsString(attr_name);
		if (strcmp(attr_str, "id") == 0) {
			retval = self->id;
			Py_INCREF(retval);
		} else {
			// note: PyDict_GetItem doesn't set error
			retval = PyDict_GetItem(self->dict, attr_name);
			if (retval == NULL) {
				(void)PyErr_Format(PyExc_AttributeError, "'pion.session' object has no attribute '%s'", attr_str);
			} else {
				Py_INCREF(retval);
			}
		}
	}
	
	return retval;
}

static int
Session_SetAttr(PyObject *obj, PyObject *attr_name, PyObject *value)
{
	int retval = -1;
	PythonSessionObject *self = (PythonSessionObject*) obj;
	char *attr_str = PyString_AsString(attr_name);

	if (strcmp(attr_str, "id") == 0) {
		(void)PyErr_Format(PyExc_AttributeError, "Read-only attribute: %s", attr_str);
		retval = -1;
	} else if (value == NULL) {
		retval = PyDict_DelItem(self->dict, attr_name);
	} else {
		retval = PyDict_SetItem(self->dict, attr_name, value);
	}
	
	return retval;
}

static PyTypeObject PythonSessionType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /*ob_size*/
	"pion.session",            /*tp_name*/
	sizeof(PythonSessionObject), /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Session_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	Session_GetAttr,           /*tp_getattro*/
	Session_SetAttr,           /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
	"pion session objects",    /* tp_doc */
	0,		                   /* tp_traverse */
	0,		                   /* tp_clear */
	0,		                   /* tp_richcompare */
	0,		                   /* tp_weaklistoffset */
	0,		                   /* tp_iter */
	0,		                   /* tp_iternext */
	0,                         /* tp_methods */
	0,                         /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)Session_init,    /* tp_init */
	0,                         /* tp_alloc */
	Session_new,               /* tp_new */
};

static PythonSessionObject *
Session_create(const Event::BlobType& session_id)
{
	PythonSessionObject *self;
	self = (PythonSessionObject *) PythonSessionType.tp_alloc(&PythonSessionType, 0);
	if (self != NULL) {
		self->id = PyString_FromString(session_id.get());
		self->dict = PyDict_New();
	}
	return self;
}


static PyMethodDef PionPythonCallbackMethods[] = {
	{NULL, NULL, 0, NULL}
};


// static members of PythonReactor
	
const string			PythonReactor::START_FUNCTION_NAME = "start";
const string			PythonReactor::STOP_FUNCTION_NAME = "stop";
const string			PythonReactor::PROCESS_FUNCTION_NAME = "process";
const string			PythonReactor::FILENAME_ELEMENT_NAME = "Filename";
const string			PythonReactor::PYTHON_SOURCE_ELEMENT_NAME = "PythonSource";
const string			PythonReactor::OPEN_SESSIONS_ELEMENT_NAME = "OpenSessions";
const string			PythonReactor::VOCAB_CLICKSTREAM_SESSION_EVENT="urn:vocab:clickstream#session-event";
const string 			PythonReactor::VOCAB_CLICKSTREAM_SESSION_ID="urn:vocab:clickstream#session-id";
boost::mutex			PythonReactor::m_init_mutex;
boost::uint32_t			PythonReactor::m_init_num = 0;
PyInterpreterState *	PythonReactor::m_interp_ptr = NULL;
boost::thread_specific_ptr<PyThreadState> *		PythonReactor::m_state_ptr = NULL;


// PythonReactor member functions

PythonReactor::PythonReactor(void)
	: Reactor(TYPE_PROCESSING),
	m_byte_code(NULL), m_module(NULL),
	m_start_func(NULL), m_stop_func(NULL), m_process_func(NULL),
	m_reactor_ptr(NULL),
	m_session_event_term_ref(Vocabulary::UNDEFINED_TERM_REF),
	m_session_id_term_ref(Vocabulary::UNDEFINED_TERM_REF)
{
	setLogger(PION_GET_LOGGER("pion.PythonReactor"));
	boost::mutex::scoped_lock init_lock(m_init_mutex);
	if (++m_init_num == 1) {
		PION_LOG_DEBUG(m_logger, "Initializing Python interpreter");
		// initialize the thread specific state pointers
		m_state_ptr = new boost::thread_specific_ptr<PyThreadState>(&PythonReactor::releaseThreadState);
		// enable optimizations
		Py_OptimizeFlag = 2;
		// initialize python interpreter
		Py_Initialize();
		// setup pion module: Reactor data types and callback functions
		PyObject *m = Py_InitModule("pion", PionPythonCallbackMethods);
		if (PyType_Ready(&PythonReactorType) < 0) {
			PION_LOG_ERROR(m_logger, "Error initializing pion.reactor data type");
		} else {
			Py_INCREF(&PythonReactorType);
			PyModule_AddObject(m, "reactor", (PyObject*) &PythonReactorType);
		}
		// setup Event data type
		if (PyType_Ready(&PythonEventType) < 0) {
			PION_LOG_ERROR(m_logger, "Error initializing pion.event data type");
		} else {
			Py_INCREF(&PythonEventType);
			PyModule_AddObject(m, "event", (PyObject*) &PythonEventType);
		}
		// setup Session data type
		if (PyType_Ready(&PythonSessionType) < 0) {
			PION_LOG_ERROR(m_logger, "Error initializing pion.session data type");
		} else {
			Py_INCREF(&PythonSessionType);
			PyModule_AddObject(m, "session", (PyObject*) &PythonSessionType);
		}
		// initialize thread support
		PyEval_InitThreads();
		// get a pointer to the global Python interpreter
		PyThreadState *thr_state_ptr = PyThreadState_Get();
		m_interp_ptr = thr_state_ptr->interp;
		PION_ASSERT(m_interp_ptr);
		// release the global lock (GIL) since PyEval_InitThreads() acquires it
		PyEval_ReleaseThread(thr_state_ptr);
		// keep track of the thread since it's been initialized inside Python
		m_state_ptr->reset(thr_state_ptr);
	}
}
	
PythonReactor::~PythonReactor()
{
	stop();

	// get the current thread state, acquire GIL and make it "active"
	PyThreadState *thr_state_ptr = PythonReactor::initThreadState();
	PyEval_AcquireThread(thr_state_ptr);

	try {
		// free the compiled byte code (if any)
		Py_XDECREF(m_reactor_ptr);
		resetPythonSymbols();

		boost::mutex::scoped_lock init_lock(m_init_mutex);
		if (--m_init_num == 0) {
			// there are no more PythonReactor instances left
			PION_LOG_DEBUG(m_logger, "Releasing Python thread states");

			// remove the current thread to make sure it does not get "released"
			m_state_ptr->release();

			// release data for all other registered threads
			delete m_state_ptr;
			m_state_ptr = NULL;
			m_interp_ptr = NULL;

			PION_LOG_DEBUG(m_logger, "Shutting down Python interpreter");
			Py_Finalize();	// note: this releases data for the current thread
		} else {
			PyEval_ReleaseThread(thr_state_ptr);
		}
	} catch (std::exception& e) {
		PyEval_ReleaseThread(thr_state_ptr);
		PION_LOG_ERROR(m_logger, e.what());
	}
}

void PythonReactor::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	// first set config options for the Reactor base class
	ConfigWriteLock cfg_lock(*this);
	Reactor::setConfig(v, config_ptr);
	
	// get string containing source code to execute (optional)
	m_source.clear();
	ConfigManager::getConfigOption(PYTHON_SOURCE_ELEMENT_NAME, m_source, config_ptr);

	// get string containing name of the source code file to execute (optional)
	m_source_file.clear();
	if (ConfigManager::getConfigOption(FILENAME_ELEMENT_NAME, m_source_file, config_ptr)) {
		PION_LOG_DEBUG(m_logger, "Loading Python source code from: " << m_source_file);
		m_source = getSourceCodeFromFile();
	}
	
	// get copy of vocabulary used to map terms to python and back
	m_vocab_ptr.reset(new Vocabulary(v));
	updateTerms(*m_vocab_ptr);

	// make sure the thread has been initialized and acquire the GIL lock
	PythonLock py_lock;

	// call the user-defined stop() function before freeing objects
	// to stop any user-defined threads that may still be running
	if (isRunning())
		callPythonStop();

	// create Reactor object to be passed to Python functions
	Py_XDECREF(m_reactor_ptr);
	m_reactor_ptr = (PyObject*) Reactor_create(this);
	if (m_reactor_ptr == NULL)
		throw InitReactorObjectException(getPythonError());

	// pre-compile the python source code to check for errors early
	compilePythonSource();
	
	// check if python reactor is running during config change
	if (isRunning()) {
		// re-initialize the Python module
		initPythonModule();

		// if there is no process function, ensure sessions are clear
		if (! m_process_func)
			flushSessions();

		// call user-defined start()
		callPythonStart();
	}
}

void PythonReactor::updateVocabulary(const Vocabulary& v)
{
	ConfigWriteLock cfg_lock(*this);
	Reactor::updateVocabulary(v);
	m_vocab_ptr.reset(new Vocabulary(v));
	updateTerms(*m_vocab_ptr);
}

void PythonReactor::updateTerms(const Vocabulary& v)
{
	m_session_event_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SESSION_EVENT);
	if (m_session_event_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SESSION_EVENT);

	m_session_id_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SESSION_ID);
	if (m_session_id_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SESSION_ID);
}

void PythonReactor::start(void)
{
	ConfigWriteLock cfg_lock(*this);
	if (! m_is_running) {
		PION_LOG_DEBUG(m_logger, "Starting reactor: " << getId());

		// make sure the thread has been initialized and acquire the GIL lock
		PythonLock py_lock;

		if (! m_source_file.empty()) {
			// make sure that the source code has not changed since last read
			string src_code = getSourceCodeFromFile();
			if (src_code != m_source) {
				PION_LOG_DEBUG(m_logger, "Reloading Python source code from: " << m_source_file);
				m_source = src_code;
				compilePythonSource();
			}
		}

		// initialize Python module code and start the reactor
		initPythonModule();
		callPythonStart();
		m_is_running = true;
	}
}

void PythonReactor::stop(void)
{
	ConfigWriteLock cfg_lock(*this);
	if (m_is_running) {
		PION_LOG_DEBUG(m_logger, "Stopping reactor: " << getId());

		// make sure the thread has been initialized and acquire the GIL lock
		PythonLock py_lock;

		// call user-defined stop() function
		callPythonStop();
		
		// release any session objects
		flushSessions();
		
		// release function pointers and imported source code module
		Py_XDECREF(m_start_func);
		Py_XDECREF(m_stop_func);
		Py_XDECREF(m_process_func);
		Py_XDECREF(m_module);
		//Py_XDECREF(m_byte_code);	// leave this alone so that re-start() works without source change
		m_start_func = m_stop_func = m_process_func = m_module = NULL;

		m_is_running = false;
	}
}

void PythonReactor::process(const EventPtr& e)
{
	if (m_process_func) {

		// make sure the thread has been initialized and acquire the GIL lock
		PythonLock py_lock;

		// generate a python Event object to use as a parameter for the process() function
		PyObject *py_event = NULL;
		try {
			py_event = Event_create(m_reactor_ptr, e, false);
		} catch (...) {
			if (PyErr_Occurred())
				PION_LOG_ERROR(m_logger, getPythonError());
			throw;
		}

		// build argument list to process() function
		// it takes two arguments:
		// the first is the Reactor class object
		// the second is a dict type representing the Event to process
		PyObject *python_args = PyTuple_New(2);
		if (! python_args) {
			Py_DECREF(py_event);
			throw InternalPythonException(getId());
		}
		Py_INCREF(m_reactor_ptr);
		PyTuple_SetItem(python_args, 0, m_reactor_ptr);
		PyTuple_SetItem(python_args, 1, py_event);	// note: py_event reference is stolen here

		// call the process() function, passing the dict as an argument
		PION_LOG_DEBUG(m_logger, "Calling Python process() function");
		PyObject *retval = PyObject_CallObject(m_process_func, python_args);
		
		// check for uncaught runtime exceptions
		if (retval == NULL && PyErr_Occurred()) {
			Py_DECREF(python_args);
			PION_LOG_ERROR(m_logger, "in process(): " << getPythonError());
		}
		Py_DECREF(python_args);
		Py_XDECREF(retval);

		// sessions map processing: check for session close events
		if (e->getType() == m_session_event_term_ref) {
			// receipt of session event signifies completion of the session
			const Event::ParameterValue *param_ptr = e->getPointer(m_session_id_term_ref);
			if (param_ptr != NULL) {
				const Event::BlobType session_id(boost::get<const Event::BlobType&>(*param_ptr));
				if (! session_id.empty()) {
					boost::mutex::scoped_lock sessions_lock(m_sessions_mutex);
					SessionMap::iterator it = m_sessions.find(session_id);
					if (it != m_sessions.end()) {
						Py_XDECREF(it->second);
						m_sessions.erase(it);
						PION_LOG_DEBUG(m_logger, "Removed completed session: " << session_id.get());
					}
				}
			}
		}

	} else {
		// no process() python function defined, just pass events through
		PION_LOG_DEBUG(m_logger, "Delivering pion event to connections");
		deliverEvent(e);
	}
}

void PythonReactor::query(std::ostream& out, const QueryBranches& branches,
	const QueryParams& qp)
{
	// basic reactor stats
	writeBeginReactorXML(out);
	writeStatsOnlyXML(out);

	// number of sessions cached
	out << '<' << OPEN_SESSIONS_ELEMENT_NAME << '>' << getNumSessions()
		<< "</" << OPEN_SESSIONS_ELEMENT_NAME << '>' << std::endl;
	
	// finish reactor stats
	writeEndReactorXML(out);
}

bool PythonReactor::deliverToConnections(PyObject *event_ptr)
{
	// getting a ConfigReadLock here introduces a deadlock condition, but
	// no need to acquire a ConfigReadLock because all python threads will
	// be stopped before any changes are ever made
	PION_LOG_DEBUG(m_logger, "Delivering python event to connections");

	// make sure that the object is of the correct type
	if (! PyObject_IsInstance(event_ptr, (PyObject*) &PythonEventType)) {
		PyErr_SetString(PyExc_TypeError, "parameter must be a pion.event");
		return false;
	}

	// don't let exceptions thrown downstream propogate up
	try {
		PythonEventObject *event_obj_ptr = (PythonEventObject*) event_ptr;
		// unset uniqueness since the event ptr may be shared
		event_obj_ptr->is_unique = false;
		// must release GIL to prevent deadlock in potential downstream PythonReactors
		PythonLock py_lock(true);	// inversed lock
		deliverEvent(event_obj_ptr->event_ptr);
	} catch (std::exception& e) {
		std::string error_msg(e.what());
		if (PyErr_Occurred()) {
			error_msg += " (";
			error_msg += getPythonError();
			error_msg += ')';
		}
		PION_LOG_ERROR(m_logger, error_msg);
	} catch (...) {
		if (PyErr_Occurred()) {
			PION_LOG_ERROR(m_logger, getPythonError());
		} else {
			PION_LOG_ERROR(m_logger, "caught unrecognized exception");
		}
	}
	
	return true;
}

PyObject *PythonReactor::getSession(PyObject *event_ptr)
{
	// make sure that the object is of the correct type
	if (! PyObject_IsInstance(event_ptr, (PyObject*) &PythonEventType)) {
		PyErr_SetString(PyExc_TypeError, "parameter must be a pion.event");
		return NULL;
	}

	// get session identifier for event
	PythonEventObject *event_obj_ptr = (PythonEventObject*) event_ptr;
	PION_ASSERT(event_obj_ptr->event_ptr.get());
	const Event& e = *(event_obj_ptr->event_ptr);
	const Event::ParameterValue *param_ptr = e.getPointer(m_session_id_term_ref);
	if (param_ptr == NULL) {
		PyErr_SetString(PyExc_TypeError, "event is missing session identifier");
		return NULL;
	}
	const Event::BlobType session_id(boost::get<const Event::BlobType&>(*param_ptr));
	if (session_id.empty()) {
		PyErr_SetString(PyExc_TypeError, "event has empty session identifier");
		return NULL;
	}

	// check to see if the session already has an object assigned
	PyObject *retval = NULL;
	boost::mutex::scoped_lock sessions_lock(m_sessions_mutex);
	SessionMap::iterator it = m_sessions.find(session_id);
	if (it == m_sessions.end()) {
		retval = (PyObject*) Session_create(session_id);
		m_sessions.insert(std::make_pair(session_id, retval));
		PION_LOG_DEBUG(m_logger, "Created new session object for " << session_id.get());
	} else {
		retval = it->second;
		PION_LOG_DEBUG(m_logger, "Using existing session object for " << session_id.get());
	}
	
	Py_XINCREF(retval);
	return retval;
}

std::size_t PythonReactor::getNumSessions(void) const
{
	boost::mutex::scoped_lock sessions_lock(m_sessions_mutex);
	return m_sessions.size();
}

void PythonReactor::flushSessions(void)
{
	boost::mutex::scoped_lock sessions_lock(m_sessions_mutex);
	size_t num_sessions = m_sessions.size();

	if (num_sessions > 0) {
		for (SessionMap::const_iterator it = m_sessions.begin();
			it != m_sessions.end(); ++it)
		{
			Py_XDECREF(it->second);
		}
		m_sessions.clear();
		PION_LOG_DEBUG(m_logger, "Flushing " << num_sessions << " session objects");
	}
}

PyThreadState *PythonReactor::initThreadState(void)
{
	// check if the thread's state has already been initialized with Python
	PyThreadState *thr_state_ptr = m_state_ptr->get();
	if (thr_state_ptr == NULL) {
		// the thread's state has not yet been initialized with Python
		thr_state_ptr = PyThreadState_New(m_interp_ptr);
		m_state_ptr->reset(thr_state_ptr);
	}
	return thr_state_ptr;
}

void PythonReactor::releaseThreadState(PyThreadState *ptr)
{
	PyThreadState_Clear(ptr);
	PyThreadState_Delete(ptr);
}

PyObject *PythonReactor::findPythonFunction(PyObject *module_ptr, const std::string& func_name)
{
	PyObject *func_ptr = PyObject_GetAttrString(module_ptr, const_cast<char*>(func_name.c_str()));
	if (func_ptr) {
		if (! PyCallable_Check(func_ptr)) {
			Py_DECREF(func_ptr);
			throw NotCallableException(func_name);
		}
		PION_LOG_DEBUG(m_logger, "Found " << func_name << "() function");
	} else {
		PyErr_Clear();
		PION_LOG_DEBUG(m_logger, "Unable to find " << func_name << "() function");
	}
	return func_ptr;
}

void PythonReactor::resetPythonSymbols(void)
{
	// assumes ConfigWriteLock and PythonLock
	PION_LOG_DEBUG(m_logger, "Resetting Python symbols");
	Py_XDECREF(m_start_func);
	Py_XDECREF(m_stop_func);
	Py_XDECREF(m_process_func);
	Py_XDECREF(m_module);
	Py_XDECREF(m_byte_code);
	m_start_func = m_stop_func = m_process_func = m_module = m_byte_code = NULL;
}

void PythonReactor::compilePythonSource(void)
{
	// assumes ConfigWriteLock and PythonLock

	// free the compiled byte code (if any)
	resetPythonSymbols();

	if (! m_source.empty()) {
		// append Pion modules to sys.path if not already there
		const char * const py_path_ptr = Py_GetPath();
		std::string py_path_str(getConfigManager().resolveRelativePath("pymodules"));
		if (py_path_ptr == NULL || strstr(py_path_ptr, py_path_str.c_str()) == NULL) {
			if (py_path_ptr && *py_path_ptr != '\0') {
				#ifdef _MSC_VER
					py_path_str += ';';
				#else
					py_path_str += ':';
				#endif
				py_path_str += py_path_ptr;
			}
			PySys_SetPath(const_cast<char*>(py_path_str.c_str()));
		}
		// compile source code into byte code
		PION_LOG_DEBUG(m_logger, "Compiling Python source code");
		m_byte_code = Py_CompileString(m_source.c_str(), m_source_file.c_str(), Py_file_input);
		if (m_byte_code == NULL) {
			throw FailedToCompileException(getPythonError());
		}
	}
}

void PythonReactor::initPythonModule(void)
{
	// assumes ConfigWriteLock and PythonLock

	if (m_byte_code) {
		PION_LOG_DEBUG(m_logger, "Initializing Python module");

		std::string modname = "pion." + getId();
		// note: Python API calls for "char*" but never will modify it (API design bug work-around)
		m_module = PyImport_ExecCodeModule(const_cast<char*>(modname.c_str()), m_byte_code);
		if (m_module == NULL) {
			Py_DECREF(m_byte_code);
			m_byte_code = NULL;
			throw FailedToCompileException(getPythonError());
		}
	
		// find start() function in Python module
		m_start_func = findPythonFunction(m_module, START_FUNCTION_NAME);

		// find stop() function in Python module
		m_stop_func = findPythonFunction(m_module, STOP_FUNCTION_NAME);

		// find process() function in Python module
		m_process_func = findPythonFunction(m_module, PROCESS_FUNCTION_NAME);
	}
}

void PythonReactor::callPythonStart(void)
{
	// assumes ConfigWriteLock and PythonLock
	if (m_start_func) {
		// execute the Python module's start() function
		PION_LOG_DEBUG(m_logger, "Calling Python start() function");
		PyObject *python_args = PyTuple_New(1);
		if (! python_args)
			throw InternalPythonException(getId());
		Py_INCREF(m_reactor_ptr);
		PyTuple_SetItem(python_args, 0, m_reactor_ptr);
		PyObject *retval = PyObject_CallObject(m_start_func, python_args);
		Py_DECREF(python_args);
	
		// check for uncaught runtime exceptions
		if (retval == NULL && PyErr_Occurred()) {
			PION_LOG_ERROR(m_logger, "in start(): " << getPythonError());
		}
	
		Py_XDECREF(retval);
	}
}

void PythonReactor::callPythonStop(void)
{
	// assumes ConfigWriteLock and PythonLock
	if (m_stop_func) {
		// execute the Python module's stop() function
		PION_LOG_DEBUG(m_logger, "Calling Python stop() function");
		PyObject *python_args = PyTuple_New(1);
		if (! python_args)
			throw InternalPythonException(getId());
		Py_INCREF(m_reactor_ptr);
		PyTuple_SetItem(python_args, 0, m_reactor_ptr);
		PyObject *retval = PyObject_CallObject(m_stop_func, python_args);
		Py_DECREF(python_args);
	
		// check for uncaught runtime exceptions
		if (retval == NULL && PyErr_Occurred()) {
			PION_LOG_ERROR(m_logger, "in stop(): " << getPythonError());
		}
	
		Py_XDECREF(retval);
	}
}

std::string PythonReactor::getSourceCodeFromFile(void)
{
	// find and confirm existance of source code file
	string src_file = getConfigManager().resolveRelativePath(m_source_file);
	if (! boost::filesystem::exists(src_file) )
		throw SourceFileNotFoundException(m_source_file);
	
	// open source code file
	ifstream src_stream(src_file.c_str(), ios::in);
	if (! src_stream.is_open())
		throw ReadSourceFileException(m_source_file);

	// read file contents into a buffer
	ostringstream str_stream;
	try {
		str_stream << src_stream.rdbuf();
		src_stream.close();
	} catch (...) {
		throw ReadSourceFileException(m_source_file);
	}

	return str_stream.str();
}

std::string PythonReactor::getPythonError(void)
{
	// assumes PythonLock
	std::string error_str;
	PyObject *ptype = NULL;
	PyObject *pvalue = NULL;
	PyObject *ptraceback = NULL;
	PyObject *psyntax = NULL;

	PyErr_Fetch(&ptype, &pvalue, &ptraceback);	// note: clears exception

	if (ptype) {
		PyTypeObject* type_obj = (PyTypeObject*) ptype;
		error_str += type_obj->tp_name;
		error_str += ": ";
	} else {
		error_str += "Exception: ";
	}

	if (pvalue) {
		if (PyErr_GivenExceptionMatches(ptype, PyExc_SyntaxError)
			&& PyTuple_Check(pvalue) && PyTuple_Size(pvalue) >= 2)
		{
			PyObject *str_obj = PyObject_Str(PyTuple_GetItem(pvalue, 0));
			if (str_obj) {
				error_str += PyString_AsString(str_obj);
				Py_DECREF(str_obj);
			}
			psyntax = PyTuple_GetItem(pvalue, 1);
		} else {
			PyObject *str_obj = PyObject_Str(pvalue);
			if (str_obj) {
				error_str += PyString_AsString(str_obj);
				Py_DECREF(str_obj);
			}
		}
	}

	if (ptraceback) {
		PyTracebackObject* traceback = (PyTracebackObject*)ptraceback;
		while (traceback->tb_next != NULL)
			traceback = traceback->tb_next;
		error_str += " (";
		const char *cptr = PyString_AsString(traceback->tb_frame->f_code->co_filename);
		if (cptr && *cptr != '\0') {
			error_str += cptr;
			error_str += " ";
		}
		error_str += "line ";
		error_str += boost::lexical_cast<std::string>(traceback->tb_lineno);
		error_str += ")";
	} else if (psyntax) {
		PyObject *str_obj = PyObject_Str(psyntax);
		if (str_obj) {
			error_str += " ";
			error_str += PyString_AsString(str_obj);
			Py_DECREF(str_obj);
		}
	}

	Py_XDECREF(ptype);
	Py_XDECREF(pvalue);
	Py_XDECREF(ptraceback);

	return error_str;
}

	
}	// end namespace plugins
}	// end namespace pion


/// creates new PythonReactor objects
extern "C" PION_PLUGIN_API pion::platform::Reactor *pion_create_PythonReactor(void) {
	return new pion::plugins::PythonReactor();
}

/// destroys PythonReactor objects
extern "C" PION_PLUGIN_API void pion_destroy_PythonReactor(pion::plugins::PythonReactor *reactor_ptr) {
	delete reactor_ptr;
}
