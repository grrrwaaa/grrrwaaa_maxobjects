print("hello mofos")

local ffi = require "ffi"
local C = ffi.C

ffi.cdef [[
typedef unsigned int GLenum;
typedef float GLfloat;
enum {
 GL_QUADS                          = 0x0007
};
void glBegin (GLenum mode);
void glEnd();
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
]]

function draw()
	print("draw")
	C.glBegin(C.GL_QUADS);
	C.glVertex3f(-1,-1,0);
	C.glVertex3f(-1,1,0);
	C.glVertex3f(1,1,0);
	C.glVertex3f(1,-1,0);
	C.glEnd();
end