%typemap(doc,name="$1",type="Dictionary") CreateWindowInfo 
"$1: { \"Size\" : [#,#], \"Title\":\"...\", \"FullScreen\":T/F, \"AllowResize\":T/F} -> CreateWindowInfo";

%typemap(in) CreateWindowInfo
{
    /* Verify is dictionary */
    if (!PyDict_Check($input)) Err("Expected positive value.");

    auto size = PyDict_GetItemString($input, "Size");
    auto title = PyDict_GetItemString($input, "Title");
    auto fs = PyDict_GetItemString($input, "FullScreen");
    auto ar = PyDict_GetItemString($input, "Resizeable");
    
    /* Verify key existance */
    if (!size) Err("Expected key Size.");
    if (!title) Err("Expected key Title.");
    if (!fs) Err("Expected key FullScreen.");
    if (!ar) Err("Expected key AllowResize.");

    /* Size is sequence of 2 ints */
    if(!PySequence_Check(size)) Err("Size: Expected a sequence of 2 ints.");
    if(PySequence_Length(size) != 2) Err("Size: Sequence must contain eactly 2 ints");

    auto x_item = PySequence_GetItem(size, 0);
    auto y_item = PySequence_GetItem(size, 1);

    if (!PyLong_Check(x_item) || !PyLong_Check(y_item)) Err("Size: Sequence must contain integers");

    if(!String_Check(title)) Err("Title: Expected a string");
    if(!PyBool_Check(fs)) Err("FullScreen: Expected a boolean");
    if(!PyBool_Check(ar)) Err("Resizable: Expected a boolean");

    CreateWindowInfo info;
    info.width = (int) PyLong_AsLong(x_item);
    info.height = (int) PyLong_AsLong(y_item);
    info.title = String_AsString(title);
    info.resizable = PyLong_AsLong(fs);
    info.fullscreen = PyLong_AsLong(ar);

    $1 = info;
}
