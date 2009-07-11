/* Sets a variety of OpenGL state, mainly to test the logger */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>
#include "test.h"

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

static void set_enables(void)
{
    glEnable(GL_LIGHTING);             test_log_printf("trace\\.call: glEnable\\(GL_LIGHTING\\)\n");
    glEnable(GL_DEPTH_TEST);           test_log_printf("trace\\.call: glEnable\\(GL_DEPTH_TEST\\)\n");
    glEnable(GL_ALPHA_TEST);           test_log_printf("trace\\.call: glEnable\\(GL_ALPHA_TEST\\)\n");
    glEnable(GL_AUTO_NORMAL);          test_log_printf("trace\\.call: glEnable\\(GL_AUTO_NORMAL\\)\n");
    glEnable(GL_TEXTURE_1D);           test_log_printf("trace\\.call: glEnable\\(GL_TEXTURE_1D\\)\n");
    glEnable(GL_TEXTURE_2D);           test_log_printf("trace\\.call: glEnable\\(GL_TEXTURE_2D\\)\n");
#ifdef GL_EXT_texture3D
    if (GLEW_EXT_texture3D)
    {
        glEnable(GL_TEXTURE_3D_EXT);   test_log_printf("trace\\.call: glEnable\\(GL_TEXTURE_3D(_EXT)?\\)\n");
    }
#endif
#ifdef GL_ARB_texture_cube_map
    if (GLEW_ARB_texture_cube_map)
    {
        glEnable(GL_TEXTURE_CUBE_MAP_ARB); test_log_printf("trace\\.call: glEnable\\(GL_TEXTURE_CUBE_MAP(_ARB)?\\)\n");
    }
#endif
    glEnable(GL_POLYGON_OFFSET_LINE);  test_log_printf("trace\\.call: glEnable\\(GL_POLYGON_OFFSET_LINE\\)\n");
    glEnable(GL_BLEND);                test_log_printf("trace\\.call: glEnable\\(GL_BLEND\\)\n");
}

void set_client_state(void)
{
    glEnableClientState(GL_VERTEX_ARRAY); test_log_printf("trace\\.call: glEnableClientState\\(GL_VERTEX_ARRAY\\)\n");
    glVertexPointer(3, GL_INT, 0, NULL);  test_log_printf("trace\\.call: glVertexPointer\\(3, GL_INT, 0, NULL\\)\n");
    glPixelStorei(GL_PACK_ALIGNMENT, 1);  test_log_printf("trace\\.call: glPixelStorei\\(GL_PACK_ALIGNMENT, 1\\)\n");
    glPixelStoref(GL_PACK_SWAP_BYTES, GL_FALSE); test_log_printf("trace\\.call: glPixelStoref\\(GL_PACK_SWAP_BYTES, GL_FALSE\\)\n");
}

void set_texture_state(void)
{
    GLuint id;
    GLint arg;

    glGenTextures(1, &id);                test_log_printf("trace\\.call: glGenTextures\\(1, %p -> { %u }\\)\n", (void *) &id, (unsigned int) id);
    glBindTexture(GL_TEXTURE_2D, id);     test_log_printf("trace\\.call: glBindTexture\\(GL_TEXTURE_2D, [0-9]+\\)\n");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("trace\\.call: glTexImage2D\\(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL\\)\n");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    test_log_printf("trace\\.call: glTexParameteri\\(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR\\)\n");
    arg = GL_LINEAR;
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &arg);
    test_log_printf("trace\\.call: glTexParameteriv\\(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, %p -> GL_LINEAR\\)\n", (void *) &arg);
#ifdef GL_SGIS_generate_mipmap
    if (GLEW_SGIS_generate_mipmap)
    {
        glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
        test_log_printf("trace\\.call: glTexParameterf\\(GL_TEXTURE_2D, GL_GENERATE_MIPMAP(_SGIS)?, GL_TRUE\\)\n");
    }
#endif
    glDeleteTextures(1, &id);
    test_log_printf("trace\\.call: glDeleteTextures\\(1, %p -> { %u }\\)\n", (void *) &id, (unsigned int) id);
}

static void set_material_state(void)
{
    GLfloat col[4] = {0.0, 0.25, 0.5, 1.0};
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
    test_log_printf("trace\\.call: glMaterialfv\\(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, %p -> { 0, 0\\.25, 0\\.5, 1 }\\)\n",
                    (void *) col);
}

static void set_matrices(void)
{
    GLfloat m[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    glMultMatrixf(m);
    test_log_printf("trace\\.call: glMultMatrixf\\(%p -> { { %g, %g, %g, %g }, { %g, %g, %g, %g }, { %g, %g, %g, %g }, { %g, %g, %g, %g } }\\)\n",
                    m,
                    m[0], m[1], m[2], m[3],
                    m[4], m[5], m[6], m[7],
                    m[8], m[9], m[10], m[11],
                    m[12], m[13], m[14], m[15]);
}

test_status setstate_suite(void)
{
    set_enables();
    set_client_state();
    set_texture_state();
    set_material_state();
    set_matrices();
    return TEST_RAN;
}
