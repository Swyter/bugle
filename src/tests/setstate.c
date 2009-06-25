/* Sets a variety of OpenGL state, mainly to test the logger */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _POSIX_SOURCE
#include <GL/glew.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>

/* Still TODO:
 * - glCompressedTexImage
 * - glLight
 * - glMaterial
 * - glClipPlane
 * - glGenBuffers, glDeleteBuffers
 * - glGenQueries, glDeleteQueries
 * - glConvolutionFilter1D
 * - glConvolutionFilter2D
 * - glSeparableFilter1D
 * - glPixelTransfer
 * - glPixelMap
 * - glColorTable
 * - glCopyColorTable
 * - glCopyColorSubTable
 * - glTexImage1D, glTexImage3D
 * - glTexSubImage1D, glTexSubImage2D, glTexSubImage3D
 * - glCopyTexImage1D, glCopyTexSubImage1D, glCopyTexImage2D, glCopyTexSubImage2D
 * - glVertexAttrib*
 * - glUniform*
 */

static FILE *ref;

static void set_enables()
{
    glEnable(GL_LIGHTING);             fprintf(ref, "trace\\.call: glEnable\\(GL_LIGHTING\\)\n");
    glEnable(GL_DEPTH_TEST);           fprintf(ref, "trace\\.call: glEnable\\(GL_DEPTH_TEST\\)\n");
    glEnable(GL_ALPHA_TEST);           fprintf(ref, "trace\\.call: glEnable\\(GL_ALPHA_TEST\\)\n");
    glEnable(GL_AUTO_NORMAL);          fprintf(ref, "trace\\.call: glEnable\\(GL_AUTO_NORMAL\\)\n");
    glEnable(GL_TEXTURE_1D);           fprintf(ref, "trace\\.call: glEnable\\(GL_TEXTURE_1D\\)\n");
    glEnable(GL_TEXTURE_2D);           fprintf(ref, "trace\\.call: glEnable\\(GL_TEXTURE_2D\\)\n");
#ifdef GL_EXT_texture3D
    if (GLEW_EXT_texture3D)
    {
        glEnable(GL_TEXTURE_3D_EXT);   fprintf(ref, "trace\\.call: glEnable\\(GL_TEXTURE_3D(_EXT)?\\)\n");
    }
#endif
#ifdef GL_ARB_texture_cube_map
    if (GLEW_ARB_texture_cube_map)
    {
        glEnable(GL_TEXTURE_CUBE_MAP_ARB); fprintf(ref, "trace\\.call: glEnable\\(GL_TEXTURE_CUBE_MAP(_ARB)?\\)\n");
    }
#endif
    glEnable(GL_POLYGON_OFFSET_LINE);  fprintf(ref, "trace\\.call: glEnable\\(GL_POLYGON_OFFSET_LINE\\)\n");
    glEnable(GL_BLEND);                fprintf(ref, "trace\\.call: glEnable\\(GL_BLEND\\)\n");
}

void set_client_state()
{
    glEnableClientState(GL_VERTEX_ARRAY); fprintf(ref, "trace\\.call: glEnableClientState\\(GL_VERTEX_ARRAY\\)\n");
    glVertexPointer(3, GL_INT, 0, NULL);  fprintf(ref, "trace\\.call: glVertexPointer\\(3, GL_INT, 0, NULL\\)\n");
    glPixelStorei(GL_PACK_ALIGNMENT, 1);  fprintf(ref, "trace\\.call: glPixelStorei\\(GL_PACK_ALIGNMENT, 1\\)\n");
    glPixelStoref(GL_PACK_SWAP_BYTES, GL_FALSE); fprintf(ref, "trace\\.call: glPixelStoref\\(GL_PACK_SWAP_BYTES, GL_FALSE\\)\n");
}

void set_texture_state()
{
    GLuint id;
    GLint arg;

    glGenTextures(1, &id);                fprintf(ref, "trace\\.call: glGenTextures\\(1, %p -> { %u }\\)\n", (void *) &id, (unsigned int) id);
    glBindTexture(GL_TEXTURE_2D, id);     fprintf(ref, "trace\\.call: glBindTexture\\(GL_TEXTURE_2D, [0-9]+\\)\n");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage2D\\(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL\\)\n");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    fprintf(ref, "trace\\.call: glTexParameteri\\(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR\\)\n");
    arg = GL_LINEAR;
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &arg);
    fprintf(ref, "trace\\.call: glTexParameteriv\\(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, %p -> GL_LINEAR\\)\n", (void *) &arg);
#ifdef GL_SGIS_generate_mipmap
    if (GLEW_SGIS_generate_mipmap)
    {
        glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
        fprintf(ref, "trace\\.call: glTexParameterf\\(GL_TEXTURE_2D, GL_GENERATE_MIPMAP(_SGIS)?, GL_TRUE\\)\n");
    }
#endif
    glDeleteTextures(1, &id);
    fprintf(ref, "trace\\.call: glDeleteTextures\\(1, %p -> { %u }\\)\n", (void *) &id, (unsigned int) id);
}

static void set_material_state()
{
    GLfloat col[4] = {0.0, 0.25, 0.5, 1.0};
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
    fprintf(ref, "trace\\.call: glMaterialfv\\(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, %p -> { 0, 0\\.25, 0\\.5, 1 }\\)\n",
            (void *) col);
}

static void set_matrices()
{
    GLfloat m[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    glMultMatrixf(m);
    fprintf(ref, "trace\\.call: glMultMatrixf\\(%p -> { { %g, %g, %g, %g }, { %g, %g, %g, %g }, { %g, %g, %g, %g }, { %g, %g, %g, %g } }\\)\n",
            m,
            m[0], m[1], m[2], m[3],
            m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11],
            m[12], m[13], m[14], m[15]);
}

int main(int argc, char **argv)
{
    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("state generator");
    glewInit();

    set_enables();
    set_client_state();
    set_texture_state();
    set_material_state();
    set_matrices();
    return 0;
}