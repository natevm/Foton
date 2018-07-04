%include <exception.i>
%include HelperFunctions.i
%include Dictionaries/Dictionaries.i

%include "std_deque.i"
%include "std_list.i"
%include "std_map.i"
%include "std_pair.i"
%include "std_set.i"
%include "std_string.i"
%include "std_vector.i"
%include "std_array.i"
%include "std_shared_ptr.i"

namespace std {
   %template(IntVector) vector<int>;
   %template(DoubleVector) vector<double>;
   %template(StringVector) vector<string>;
};
