#ifndef ICED_H

#include "gl3w.h"

void iced_init(void);
void iced_gui(void);
void iced_render(void);

static inline const char* gl_err_string(GLenum err)
{
	switch (err) {
	#define X(NAME) case NAME: return #NAME;
	X(GL_NO_ERROR)
	X(GL_INVALID_ENUM)
	X(GL_INVALID_VALUE)
	X(GL_INVALID_OPERATION)
	X(GL_STACK_OVERFLOW)
	X(GL_STACK_UNDERFLOW)
	X(GL_OUT_OF_MEMORY)
	#undef X
	default: return "???";
	}
}

#define CHKGL { GLenum xx_GLERR = glGetError(); if (xx_GLERR != GL_NO_ERROR) { fprintf(stderr, "OPENGL ERROR 0x%.4x (%s) in %s:%d\n", xx_GLERR, gl_err_string(xx_GLERR), __FILE__, __LINE__); abort(); } }

#define ICED_H
#endif
