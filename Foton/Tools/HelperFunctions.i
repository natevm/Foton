%{

inline bool String_Check(PyObject *object) {
    return PyString_Check(object) || PyUnicode_Check(object);
}

inline std::string String_AsString(PyObject *object) {
    if (PyString_Check(object)) 
        return std::string(PyString_AsString(object));
    else if (PyUnicode_Check(object)){
        PyObject * ascii_object=PyUnicode_AsASCIIString(object);
        return std::string(PyBytes_AsString(ascii_object));
    }
    else return "";
}

#define Err(message) { SWIG_Python_SetErrorMsg(PyExc_ValueError, message); return NULL; }
%}