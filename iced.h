#ifndef ICED_H

void iced_init(void);
void iced_gui(void);
void iced_render(void);

#define CHKGL { GLenum xx_GLERR = glGetError(); if (xx_GLERR != GL_NO_ERROR) { fprintf(stderr, "OPENGL ERROR 0x%.4x in %s:%d\n", xx_GLERR, __FILE__, __LINE__); abort(); } }

#define ICED_H
#endif
