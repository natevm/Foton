/* Common Input Attributes */
layout(location = 0) in vec3 point;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

/* Outputs */
layout(location = 0) out vec3 w_normal;
layout(location = 1) out vec3 w_position;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 w_cameraPos;
layout(location = 4) out vec4 vert_color;
layout(location = 5) out vec3 m_position;
layout(location = 6) out vec3 s_position;

invariant gl_Position;