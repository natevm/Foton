%include <exception.i>
%include HelperFunctions.i
%include Dictionaries/Dictionaries.i

%include "std_deque.i"
%include "std_list.i"
%include "std_map.i"
%include "std_unordered_map.i"
%include "std_pair.i"
%include "std_set.i"
%include "std_string.i"
%include "std_vector.i"
%include "std_array.i"
%include "std_shared_ptr.i"

%include "stdint.i"

namespace std {
   %template(UInt32Vector) vector<uint32_t>;
   %template(UInt16Vector) vector<uint16_t>;
   %template(UInt8Vector) vector<uint8_t>;

   %template(Int32Vector) vector<int32_t>;
   %template(Int16Vector) vector<int16_t>;
   %template(Int8Vector) vector<int8_t>;

   %template(DoubleVector) vector<double>;
   %template(FloatVector) vector<float>;
   %template(StringVector) vector<string>;

   %template(IntSet) set<int>;
   %template(DoubleSet) set<double>;
   %template(FloatSet) set<float>;
   %template(StringSet) set<string>;
};