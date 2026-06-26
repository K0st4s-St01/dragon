#ifndef GLAD_Glad_H
#define GLAD_Glad_H

#include <glad/gl.h>

#define GLADloadproc GLADloadfunc
#define gladLoadGLLoader(load) gladLoadGL((GLADloadfunc)(load))

#endif
