#version 450
#include "Pluto/Resources/Shaders/Descriptors.hxx"
#include "Pluto/Resources/Shaders/Attributes.hxx"

void main() {
    /* Epsilon offset to account for depth precision errors values */
    #ifndef DISABLE_REVERSE_Z
    gl_Position = vec4(point.xy, 1.0, 1.0);
    #else
    gl_Position = vec4(point.xy, 0.0, 1.0);
    #endif
}
