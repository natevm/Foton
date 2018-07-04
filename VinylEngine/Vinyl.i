%module Vinyl

%{
#include "Vinyl/Vinyl.hxx"
%}

%feature("autodoc","2");

%include "Typemaps/Typemaps.i"

%ignore threadFunction;
%include "Vinyl.hxx"

%pythoncode %{
import json

with open("./config.json", "r") as read_file:
    config = json.load(read_file)

def Init(configuration = config) :
    InitWindow(config["Window"])
%}

