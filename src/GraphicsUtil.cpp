/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
** Copyright (c) 2010, Monash University
** All rights reserved.
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
**       * Redistributions of source code must retain the above copyright notice,
**          this list of conditions and the following disclaimer.
**       * Redistributions in binary form must reproduce the above copyright
**         notice, this list of conditions and the following disclaimer in the
**         documentation and/or other materials provided with the distribution.
**       * Neither the name of the Monash University nor the names of its contributors
**         may be used to endorse or promote products derived from this software
**         without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
** THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
** OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**
** Contact:
*%  Owen Kaluza - Owen.Kaluza(at)monash.edu
*%
*% Development Team :
*%  http://www.underworldproject.org/aboutus.html
**
**~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#if defined HAVE_LIBPNG or defined _WIN32
#include <png.h>
#include <zlib.h>
#endif

#include "GraphicsUtil.h"
#include  "base64.h"
#include <string.h>
#include <math.h>

#ifdef HAVE_GL2PS
#include <gl2ps.h>
#endif

void compareCoordMinMax(float* min, float* max, float *coord)
{
  for (int i=0; i<3; i++)
  {
    assert(!std::isnan(coord[i]));
    if (std::isinf(coord[i])) return;
    if (coord[i] > max[i] && coord[i] < HUGE_VAL)
    {
      max[i] = coord[i];
      //std::cerr << "Updated MAX: " << Vec3d(max) << std::endl;
      //getchar();
    }
    if (coord[i] < min[i] && coord[i] > -HUGE_VAL)
    {
      min[i] = coord[i];
      //std::cerr << "Updated MIN: " << Vec3d(min) << std::endl;
      //getchar();
    }
  }
}

void clearMinMax(float* min, float* max)
{
  for (int i=0; i<3; i++)
  {
    min[i] = HUGE_VAL;
    max[i] = -HUGE_VAL;
  }
}

void getCoordRange(float* min, float* max, float* dims)
{
  for (int i=0; i<3; i++)
  {
    dims[i] = max[i] - min[i];
  }
}

const char* glErrorString(GLenum errorCode)
{
  switch (errorCode)
  {
  case GL_NO_ERROR:
    return "No error";
  case GL_INVALID_ENUM:
    return "Invalid enumerant";
  case GL_INVALID_VALUE:
    return "Invalid value";
  case GL_INVALID_OPERATION:
    return "Invalid operation";
  case GL_STACK_OVERFLOW:
    return "Stack overflow";
  case GL_STACK_UNDERFLOW:
    return "Stack underflow";
  case GL_OUT_OF_MEMORY:
    return "Out of memory";
  }
  return "Unknown error";
}

int gluProjectf(float objx, float objy, float objz, float *windowCoordinate)
{
  //https://www.opengl.org/wiki/GluProject_and_gluUnProject_code
  int viewport[4];
  float projection[16], modelview[16];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetFloatv(GL_PROJECTION_MATRIX, projection);
  glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
  return gluProjectf(objx, objy, objz, modelview, projection, viewport, windowCoordinate);
}

int gluProjectf(float objx, float objy, float objz, float* modelview, float*projection, int* viewport, float *windowCoordinate)
{
  //https://www.opengl.org/wiki/GluProject_and_gluUnProject_code
  //Transformation vectors
  float fTempo[8];
  //Modelview transform
  fTempo[0]=modelview[0]*objx+modelview[4]*objy+modelview[8]*objz+modelview[12];  //w is always 1
  fTempo[1]=modelview[1]*objx+modelview[5]*objy+modelview[9]*objz+modelview[13];
  fTempo[2]=modelview[2]*objx+modelview[6]*objy+modelview[10]*objz+modelview[14];
  fTempo[3]=modelview[3]*objx+modelview[7]*objy+modelview[11]*objz+modelview[15];
  //Projection transform, the final row of projection matrix is always [0 0 -1 0]
  //so we optimize for that.
  fTempo[4]=projection[0]*fTempo[0]+projection[4]*fTempo[1]+projection[8]*fTempo[2]+projection[12]*fTempo[3];
  fTempo[5]=projection[1]*fTempo[0]+projection[5]*fTempo[1]+projection[9]*fTempo[2]+projection[13]*fTempo[3];
  fTempo[6]=projection[2]*fTempo[0]+projection[6]*fTempo[1]+projection[10]*fTempo[2]+projection[14]*fTempo[3];
  fTempo[7]=-fTempo[2];
  //The result normalizes between -1 and 1
  if(fTempo[7]==0.0)   //The w value
    return 0;
  fTempo[7]=1.0/fTempo[7];
  //Perspective division
  fTempo[4]*=fTempo[7];
  fTempo[5]*=fTempo[7];
  fTempo[6]*=fTempo[7];
  //Window coordinates
  //Map x, y to range 0-1
  windowCoordinate[0]=(fTempo[4]*0.5+0.5)*viewport[2]+viewport[0];
  windowCoordinate[1]=(fTempo[5]*0.5+0.5)*viewport[3]+viewport[1];
  //This is only correct when glDepthRange(0.0, 1.0)
  windowCoordinate[2]=(1.0+fTempo[6])*0.5;   //Between 0 and 1
  return 1;
}

/*
** Modified from MESA GLU 9.0.0 src/libutil/project.c
** (SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008))
** License URL as required: http://oss.sgi.com/projects/FreeB/
**
** Invert 4x4 matrix.
** Contributed by David Moore (See Mesa bug #6748)
*/
bool gluInvertMatrixf(const float m[16], float invOut[16])
{
  float inv[16], det;
  int i;

  inv[0] =   m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
             + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
  inv[4] =  -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
            - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
  inv[8] =   m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
             + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
  inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
            - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
  inv[1] =  -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
            - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
  inv[5] =   m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
             + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
  inv[9] =  -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
            - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
  inv[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
             + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
  inv[2] =   m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
             + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
  inv[6] =  -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
            - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
  inv[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
             + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
  inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
            - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
  inv[3] =  -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
            - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
  inv[7] =   m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
             + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
  inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
            - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
  inv[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
             + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

  det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
  if (det == 0)
    return false;

  det = 1.0 / det;

  for (i = 0; i < 16; i++)
    invOut[i] = inv[i] * det;

  return true;
}

void Viewport2d(int width, int height)
{
  if (width && height)
  {
    // Set up 2D Viewer the size of the viewport
    glPushAttrib(GL_ENABLE_BIT);
    glDisable( GL_DEPTH_TEST );
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    //Left, right, bottom, top, near, far
    glOrtho(0.0, (GLfloat) width, 0.0, (GLfloat) height, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Disable lighting
    glDisable(GL_LIGHTING);
    // Disable line smoothing in 2d mode
    glDisable(GL_LINE_SMOOTH);
  }
  else
  {
    // Restore settings
    glPopAttrib();
    //glEnable(GL_LINE_SMOOTH);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
  }
}

#ifdef USE_FONTS
void FontManager::setFont(Properties& properties, std::string def, float scaling, float multiplier2d)
{
  //fixed, small, sans, serif, vector
#ifdef USE_OMEGALIB
  //Always use 3D vector font
  std::string fonttype = "vector";
#else
  std::string fonttype = def;
  if (properties.has("font")) fonttype = properties["font"];
#endif
  fontscale = properties.getFloat("fontscale", scaling);

  //Colour
  Colour colour = Colour(properties["fontcolour"]);
  if (colour.a > 0.0) //Otherwise (default) leave colour unchanged
    glColor3ubv(colour.rgba);

  //Bitmap fonts
  if (fonttype == "fixed")
    charset = FONT_FIXED;
  else if (fonttype == "sans")
    charset = FONT_NORMAL;
  else if (fonttype == "serif")
    charset = FONT_SERIF;
  else if (fonttype == "vector")
    charset = FONT_VECTOR;
  else  //Default (small)
    charset = FONT_SMALL;

  //For non-vector fonts
  if (charset > FONT_VECTOR)
    fontscale *= multiplier2d;
}

void FontManager::printString(const char* str)
{
  if (charLists == 0 || !glIsList(charLists+1))   // Load font if not yet done
    charLists = GenerateFontCharacters();

  glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glListBase(charLists - 32);      // Set font display list base (space)
  glCallLists(strlen(str), GL_UNSIGNED_BYTE, str);      // Display
}

char buffer[4096];
void FontManager::printf(int x, int y, const char *fmt, ...)
{
  va_list ap;                 // Pointer to arguments list
  if (fmt == NULL) return;    // No format string
  va_start(ap, fmt);          // Parse format string for variables
  vsprintf(buffer, fmt, ap);    // Convert symbols
  va_end(ap);

  print(x, y, buffer);   // FontManager::print result string
}

void FontManager::print(int x, int y, const char *str)
{
  if (charset > FONT_VECTOR) return rasterPrint(x, y, str);
  glPushMatrix();
  glTranslated(x, y, 0);
  glScaled(fontscale, fontscale, 1.0);
  printString(str);
  glPopMatrix();
}

void FontManager::print3d(double x, double y, double z, const char *str)
{
  if (charset > FONT_VECTOR) return rasterPrint3d(x, y, z, str);
  glPushMatrix();
  glTranslated(x, y, z);
  glScaled(fontscale * FONT_SCALE_3D, fontscale * FONT_SCALE_3D, fontscale * FONT_SCALE_3D);
  printString(str);
  glPopMatrix();
}

void FontManager::print3dBillboard(double x, double y, double z, const char *str, int align)
{
  if (charset > FONT_VECTOR) return rasterPrint3d(x, y, z, str, align > -1);
  float modelview[16];
  int i,j;

  float sw = FONT_SCALE_3D * printWidth(str);
  //Default align = -1 (Left)
  if (align == 1) x -= sw;     //Right
  if (align == 0) x -= sw*0.5; //Centre
  z -= 0.025 * fontscale;

  // save the current modelview matrix
  glPushMatrix();
  glTranslated(x, y, z);

  // get the current modelview matrix
  glGetFloatv(GL_MODELVIEW_MATRIX , modelview);

  // undo all rotations
  // beware all scaling is lost as well
  for( i=0; i<3; i++ )
    for( j=0; j<3; j++ )
    {
      if ( i==j )
        modelview[i*4+j] = 1.0;
      else
        modelview[i*4+j] = 0.0;
    }

  // set the modelview with no rotations and scaling
  glLoadMatrixf(modelview);

  glScaled(fontscale * FONT_SCALE_3D, fontscale * FONT_SCALE_3D, fontscale * FONT_SCALE_3D);
  printString(str);

  // restores the modelview matrix
  glPopMatrix();
}

// String width calc
int FontManager::printWidth(const char *string)
{
  if (charset > FONT_VECTOR) return rasterPrintWidth(string);
  // Sum character widths in string
  int i, len = 0, slen = strlen(string);
  for (i = 0; i < slen; i++)
    len += font_charwidths[string[i]-32];

  // Additional pixel of spacing for each character
  float w = len + slen;
  return fontscale * w;
}

//Bitmap font stuff
void FontManager::rasterPrintString(const char* str)
{
  if (fontbase == 0)                        /* Load font if not yet done */
    rasterSetupFonts();

  if (charset > FONT_SERIF || charset < FONT_FIXED)      /* Character set valid? */
    charset = FONT_FIXED;

  /* First save state of enable flags */
  glPushAttrib(GL_ENABLE_BIT);
  glDisable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  glEnable(GL_TEXTURE_2D);                            /* Enable Texture Mapping */
  glBindTexture(GL_TEXTURE_2D, fonttexture);
  glListBase(fontbase - 32 + (96 * charset));      /* Choose the font and charset */
  glPushMatrix();
  if (fontscale >= 1.0) //Don't allow downscaling bitmap fonts
    glScalef(fontscale, fontscale, fontscale);
  glCallLists(strlen(str),GL_UNSIGNED_BYTE, str);      /* Display */
  glDisable(GL_TEXTURE_2D);                           /* Disable Texture Mapping */

  glPopMatrix();
  glPopAttrib();
}

void FontManager::rasterPrint(int x, int y, const char *str)
{
#ifdef HAVE_GL2PS
  int mode;
  glGetIntegerv(GL_RENDER_MODE, &mode);
  if (mode == GL_FEEDBACK)
  {
    /* call to gl2pText is required for text output using vector formats,
     * as no text is stored in the GL feedback buffer */
    glRasterPos2d(x, y+bmpfont_charheights[charset]);
    switch (charset)
    {
    case FONT_FIXED:
      gl2psText( str, "Courier", 12);
      break;
    case FONT_SMALL:
      gl2psText( str, "Helvetica", 8);
      break;
    case FONT_NORMAL:
      gl2psText( str, "Helvetica", 14);
      break;
    case FONT_SERIF:
      gl2psText( str, "Times-Roman", 14);
      break;
    }
    return;
  }
#endif

  glPushMatrix();
  //glLoadIdentity();
  glTranslated(x, y-bmpfont_charheights[charset], 0);
  rasterPrintString(str);
  glPopMatrix();
}

void FontManager::rasterPrint3d(double x, double y, double z, const char *str, bool alignRight)
{
  /* Calculate projected screen coords in viewport */
  float pos[3];
  GLint viewportArray[4];
  glGetIntegerv(GL_VIEWPORT, viewportArray);
  gluProjectf(x, y, z, pos);

  /* Switch to ortho view with 1 unit = 1 pixel and print using calculated screen coords */
  Viewport2d(viewportArray[2], viewportArray[3]);
  glDepthFunc(GL_ALWAYS);
  glAlphaFunc(GL_GREATER, 0.25);
  glEnable(GL_ALPHA_TEST);

  /* FontManager::print at calculated position, compensating for viewport offset */
  int xs, ys;
  xs = (int)(pos[0]) - viewportArray[0];
  if (alignRight) xs -= rasterPrintWidth(str);
  ys = (int)(pos[1]) - viewportArray[1]; //(viewportArray[3] - (yPos - viewportArray[1]));
  rasterPrint(xs, ys, str);

  /* Restore state */
  Viewport2d(0, 0);
  /* Put back settings */
  glDepthFunc(GL_LESS);
  glDisable(GL_ALPHA_TEST);
}

/* String width calc */
int FontManager::rasterPrintWidth(const char *string)
{
  /* Sum character widths in string */
  int i, len = 0, slen = strlen(string);
  for (i = 0; i < slen; i++)
    len += bmpfont_charwidths[string[i]-32 + (96 * charset)];
  /* Additional pixel of spacing for each character */
  float w = len + slen;
  if (fontscale >= 1.0) return fontscale * w;
  return w;
}

void FontManager::rasterSetupFonts()
{
  /* Load font bitmaps and Convert To Textures */
  int i, j;
  unsigned char* pixel_data = new unsigned char[IMAGE_HEIGHT * IMAGE_WIDTH * IMAGE_BYTES_PER_PIXEL];
  unsigned char fontdata[IMAGE_HEIGHT][IMAGE_WIDTH];   /* font texture data */

  /* Get font pixels from source data - interpret RGB (greyscale) as alpha channel */
  IMAGE_RUN_LENGTH_DECODE(pixel_data, IMAGE_RLE_PIXEL_DATA, IMAGE_WIDTH * IMAGE_HEIGHT, IMAGE_BYTES_PER_PIXEL);
  for (i = 0; i < IMAGE_HEIGHT; i++)
    for (j = 0; j < IMAGE_WIDTH; j++)
      fontdata[ i ][ j ] = 255 - pixel_data[ IMAGE_BYTES_PER_PIXEL * (IMAGE_WIDTH * i + j) ];

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  /* create and bind texture */
  glGenTextures(1, &fonttexture);
  glBindTexture(GL_TEXTURE_2D, fonttexture);
  glEnable(GL_COLOR_MATERIAL);
  /* use linear filtering */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  /* generate the texture from bitmap alpha data */
  glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, IMAGE_WIDTH, IMAGE_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, fontdata);
  fontbase = glGenLists(BMP_GLYPHS);

  /* Build font display lists */
  rasterBuildFont(16, 16, 0, 384);      /* 16x16 glyphs, 16 columns - 4 fonts */
  delete [] pixel_data;
}

void FontManager::rasterBuildFont(int glyphsize, int columns, int startidx, int stopidx)
{
  /* Build font display lists */
  int i;
  static float yoffset;
  float divX = IMAGE_WIDTH / (float)glyphsize;
  float divY = IMAGE_HEIGHT / (float)glyphsize;
  float glyphX = 1 / divX;   /* Width & height of a glyph in texture coords */
  float glyphY = 1 / divY;
  GLfloat cx, cy;         /* the character coordinates in our texture */
  if (startidx == 0) yoffset = 0;
  glBindTexture(GL_TEXTURE_2D, fonttexture);
  for (i = 0; i < (stopidx - startidx); i++)
  {
    cx = (float) (i % columns) / divX;
    cy = yoffset + (float) (i / columns) / divY;
    glNewList(fontbase + startidx + i, GL_COMPILE);
    glBegin(GL_QUADS);
    glTexCoord2f(cx, cy + glyphY);
    glVertex2i(0, 0);
    glTexCoord2f(cx + glyphX, cy + glyphY);
    glVertex2i(glyphsize, 0);
    glTexCoord2f(cx + glyphX, cy);
    glVertex2i(glyphsize, glyphsize);
    glTexCoord2f(cx, cy);
    glVertex2i(0, glyphsize);
    glEnd();
    /* Shift right width of character + 1 */
    glTranslated(bmpfont_charwidths[startidx + i]+1, 0, 0);
    glEndList();
  }
  /* Save vertical offset to resume from */
  yoffset = cy + glyphY;
}


#else //USE_FONTS
void FontManager::setFont(Properties& properties, std::string def, float scaling, float multiplier2d) {}
void FontManager::printString(const char* str) {}
void FontManager::printf(int x, int y, const char *fmt, ...) {}
void FontManager::print(int x, int y, const char *str) {}
void FontManager::print3d(double x, double y, double z, const char *str) {}
void FontManager::print3dBillboard(double x, double y, double z, const char *str, int align) {}
int FontManager::printWidth(const char *string)
{
  return 0;
}
void FontManager::rasterPrintString(const char* str) {}
void FontManager::rasterPrint(int x, int y, const char *str) {}
void FontManager::rasterPrint3d(double x, double y, double z, const char *str, bool alignRight) {}
int FontManager::rasterPrintWidth(const char *string)
{
  return 0;
}
void FontManager::rasterSetupFonts() {}
void FontManager::rasterBuildFont(int glyphsize, int columns, int startidx, int stopidx) {}
#endif //USE_FONTS

//Vector ops

// vectorNormalise calculates the magnitude of a vector
// \hat v = frac{v} / {|v|}
// This function uses function dotProduct to calculate v . v
void vectorNormalise(float vector[3])
{
  float mag;
  mag = sqrt(dotProduct(vector,vector));
  vector[2] = vector[2]/mag;
  vector[1] = vector[1]/mag;
  vector[0] = vector[0]/mag;
}

std::ostream & operator<<(std::ostream &os, const Vec3d& vec)
{
  return os << "[" << vec.x << "," << vec.y << "," << vec.z << "]";
}

// Given three points which define a plane, returns a vector which is normal to that plane
Vec3d vectorNormalToPlane(float pos0[3], float pos1[3], float pos2[3])
{
  Vec3d vector0 = Vec3d(pos0);
  Vec3d vector1 = Vec3d(pos1);
  Vec3d vector2 = Vec3d(pos2);

  vector1 -= vector0;
  vector2 -= vector0;

  return vector1.cross(vector2);
}

// Given three points which define a plane, NormalToPlane will give the unit vector which is normal to that plane
// Uses vectorSubtract, crossProduct and VectorNormalise
void normalToPlane( float normal[3], float pos0[3], float pos1[3], float pos2[3])
{
  float vector1[3], vector2[3];

//printf(" PLANE: %f,%f,%f - %f,%f,%f - %f,%f,%f\n", pos0[0], pos0[1], pos0[2], pos1[0], pos1[1], pos1[2], pos2[0], pos2[1], pos2[2]);
  vectorSubtract(vector1, pos1, pos0);
  vectorSubtract(vector2, pos2, pos0);

  crossProduct(normal, vector1, vector2);
//printf(" %f,%f,%f x %f,%f,%f == %f,%f,%f\n", vector1[0], vector1[1], vector1[2], vector2[0], vector2[1], vector2[2], normal[0], normal[1], normal[2]);

  //vectorNormalise( normal);
}

// Given 3 x 3d vertices defining a triangle, calculate the inner angle at the first vertex
float triAngle(float v0[3], float v1[3], float v2[3])
{
  //Returns angle at v0 in radians for triangle defined by v0,v1,v2

  //Get lengths of each side of triangle adjacent to this vertex
  float e0[3], e1[3]; //Triangle edge vectors
  vectorSubtract(e0, v1, v0);
  vectorSubtract(e1, v2, v0);

  //Normalise to simplify dot product calc
  vectorNormalise(e0);
  vectorNormalise(e1);
  //Return triangle angle (in radians)
  return acos(dotProduct(e0,e1));
}

void RawImageFlip(void* image, int width, int height, int channels)
{
  int scanline = channels * width;
  GLubyte* ptr1 = (GLubyte*)image;
  GLubyte* ptr2 = ptr1 + scanline * (height-1);
  GLubyte* temp = new GLubyte[scanline];
  for (int y=0; y<height/2; y++)
  {
    memcpy(temp, ptr1, scanline);
    memcpy(ptr1, ptr2, scanline);
    memcpy(ptr2, temp, scanline);
    ptr1 += scanline;
    ptr2 -= scanline;
  }
  delete[] temp;
}

GLubyte* RawImageCrop(void* image, int width, int height, int channels, int outwidth, int outheight, int offsetx, int offsety)
{
  int scanline = channels * width;
  int outscanline = channels * outwidth;
  GLubyte* crop = new GLubyte[outscanline*outheight];
  GLubyte* ptr1 = (GLubyte*)image + offsety*scanline + offsetx*channels;
  GLubyte* ptr2 = crop;
  for (int y=offsety; y<offsety+outheight; y++)
  {
    memcpy(ptr2, ptr1, outscanline);
    ptr1 += scanline;
    ptr2 += outscanline;
  }
  return crop;
}

TextureData* ImageLoader::use()
{
  load();

  if (texture && texture->width)
  {
    if (texture->depth > 1)
    {
      glEnable(GL_TEXTURE_3D);
      glActiveTexture(GL_TEXTURE0 + texture->unit);
      glBindTexture(GL_TEXTURE_3D, texture->id);
    }
    else
    {
      glEnable(GL_TEXTURE_2D);
      glActiveTexture(GL_TEXTURE0 + texture->unit);
      glBindTexture(GL_TEXTURE_2D, texture->id);
    }
    //printf("USE TEXTURE: (id %d unit %d)\n", texture->id, texture->unit);
    return texture;
  }

  //No texture:
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_TEXTURE_3D);
  return NULL;
}

void ImageLoader::load()
{
  //Already loaded
  if (texture) return;

  //No file, requires manual load
  if (fn.full.length() == 0) return;

  //Load texture file
  GLubyte* imageData = read();
  //Build texture
  build(imageData);
  //Dispose of data
  delete[] imageData;
}

GLubyte* ImageLoader::read()
{
  //Load image file
  texture = new TextureData();
  GLubyte* imageData = NULL;
  if (fn.type == "jpg" || fn.type == "jpeg")
    imageData = loadJPEG();
  if (fn.type == "png")
    imageData = loadPNG();
  if (fn.type == "ppm")
    imageData = loadPPM();
  if (fn.type == "tif" || fn.type == "tiff")
    imageData = loadTIFF();
  return imageData;
}

// Loads a PPM image
GLubyte* ImageLoader::loadPPM()
{
  bool readTag = false, readWidth = false, readHeight = false, readColourCount = false;
  char stringBuffer[241];
  int ppmType, colourCount;
  GLubyte *imageData;

  FILE* imageFile = fopen(fn.full.c_str(), "rb");
  if (imageFile == NULL)
  {
    debug_print("Cannot open '%s'\n", fn.full.c_str());
    return 0;
  }

  while (!readTag || !readWidth || !readHeight || !readColourCount)
  {
    // Read in a new line from file
    char* charPtr = fgets( stringBuffer, 240, imageFile );
    assert ( charPtr );

    for (charPtr = stringBuffer ; charPtr < stringBuffer + 240 ; charPtr++ )
    {
      // Check if we should go to a new line - this will happen for comments, line breaks and terminator characters
      if ( *charPtr == '#' || *charPtr == '\n' || *charPtr == '\0' )
        break;

      // Check if this is a space - if this is the case, then go to next line
      if ( *charPtr == ' ' || *charPtr == '\t' )
        continue;

      if ( !readTag )
      {
        sscanf( charPtr, "P%d", &ppmType );
        readTag = true;
      }
      else if ( !readWidth )
      {
        sscanf( charPtr, "%u", &texture->width );
        readWidth = true;
      }
      else if ( !readHeight )
      {
        sscanf( charPtr, "%u", &texture->height );
        readHeight = true;
      }
      else if ( !readColourCount )
      {
        sscanf( charPtr, "%d", &colourCount );
        readColourCount = true;
      }

      // Go to next white space
      charPtr = strpbrk( charPtr, " \t" );

      // If there are no more characters in line then go to next line
      if ( charPtr == NULL )
        break;
    }
  }

  // Only allow PPM images of type P6 and with 256 colours
  if ( ppmType != 6 || colourCount != 255 ) abort_program("Unable to load PPM Texture file, incorrect format");

  texture->channels = 3;
  imageData = new GLubyte[texture->width*texture->height*texture->channels];

  //Flip scanlines vertically
  for (int j = texture->height - 1 ; j >= 0 ; j--)
    if (fread(&imageData[texture->width * j * texture->channels], texture->channels, texture->width, imageFile) < texture->width) 
      abort_program("PPM Read Error");
  fclose(imageFile);
  return imageData;
}

GLubyte* ImageLoader::loadPNG()
{
  GLubyte *imageData;

  std::ifstream file(fn.full.c_str(), std::ios::binary);
  if (!file)
  {
    debug_print("Cannot open '%s'\n", fn.full.c_str());
    return 0;
  }
  imageData = (GLubyte*)read_png(file, texture->channels, texture->width, texture->height);

  //Requires flip on load for OpenGL
  RawImageFlip(imageData, texture->width, texture->height, texture->channels);

  file.close();

  return imageData;
}

GLubyte* ImageLoader::loadJPEG()
{
  int width, height, channels;
  GLubyte* imageData = (GLubyte*)jpgd::decompress_jpeg_image_from_file(fn.full.c_str(), &width, &height, &channels, 3);

  //Requires flip on load for OpenGL
  RawImageFlip(imageData, width, height, 3);

  texture->width = width;
  texture->height = height;
  texture->channels = channels;

  return imageData;
}

GLubyte* ImageLoader::loadTIFF()
{
  GLubyte* imageData = NULL;
#ifdef HAVE_LIBTIFF
  TIFF* tif = TIFFOpen(fn.full.c_str(), "r");
  if (tif)
  {
    unsigned int width, height, channels;
    size_t npixels;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
    npixels = width * height;
    texture->channels = 4;
    imageData = new GLubyte[npixels * texture->channels * sizeof(GLubyte)];   // Reserve Memory
    if (imageData)
    {
      if (TIFFReadRGBAImage(tif, width, height, (uint32*)imageData, 0))
      {
        RawImageFlip(imageData, width, height, texture->channels);
        texture->width = width;
        texture->height = height;
      }
    }
    TIFFClose(tif);
  }
#else
  abort_program("[Load Texture] Require libTIFF to load TIFF images\n");
#endif
  return imageData;
}

int ImageLoader::build(GLubyte* imageData)
{
  GLenum format = texture->channels == 3 ? GL_RGB : GL_RGBA;
  //Build texture from raw data
  glActiveTexture(GL_TEXTURE0 + texture->unit);
  glBindTexture(GL_TEXTURE_2D, texture->id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  // use linear filtering
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (mipmaps)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);    //set so texImage2d will gen mipmaps
  }
  else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  //Load the texture data based on bits per pixel
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  switch (texture->channels)
  {
  case 1:
    if (!format) format = GL_ALPHA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texture->width, texture->height, 0, format, GL_UNSIGNED_BYTE, imageData);
    break;
  case 2:
    if (!format) format = GL_LUMINANCE_ALPHA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texture->width, texture->height, 0, format, GL_UNSIGNED_BYTE, imageData);
    break;
  case 3:
    if (!format) format = GL_BGR;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture->width, texture->height, 0, format, GL_UNSIGNED_BYTE, imageData);
    break;
  case 4:
    if (!format) format = GL_BGRA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0, format, GL_UNSIGNED_BYTE, imageData);
    break;
  }

  return 1;
}

void ImageLoader::load3D(int width, int height, int depth, void* data, int voltype)
{
  //Save the type
  type = voltype;
  GL_Error_Check;
  //Create the texture
  if (!texture) texture = new TextureData();
  GL_Error_Check;
  //Hard coded unit for 3d textures for now
  texture->unit = 1;

  glActiveTexture(GL_TEXTURE0 + texture->unit);
  GL_Error_Check;
  glBindTexture(GL_TEXTURE_3D, texture->id);
  GL_Error_Check;

  texture->width = width;
  texture->height = height;
  texture->depth = depth;

  // set the texture parameters
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  GL_Error_Check;

  //Load based on type
  debug_print("Volume Texture: width %d height %d depth %d type %d\n", width, height, depth, type);
  switch (type)
  {
  case VOLUME_FLOAT:
    //glTexImage3D(GL_TEXTURE_3D, 0, GL_INTENSITY, width, height, depth, 0, GL_LUMINANCE, GL_FLOAT, data);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, width, height, depth, 0, GL_LUMINANCE, GL_FLOAT, data);
    break;
  case VOLUME_BYTE:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, width, height, depth, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_BYTE_COMPRESSED:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_COMPRESSED_RED, width, height, depth, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGB:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, width, height, depth, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGB_COMPRESSED:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_COMPRESSED_RGB, width, height, depth, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGBA:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, width, height, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGBA_COMPRESSED:
    glTexImage3D(GL_TEXTURE_3D, 0,  GL_COMPRESSED_RGBA, width, height, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    break;
  }
  GL_Error_Check;
}

void ImageLoader::load3Dslice(int slice, void* data)
{
  GL_Error_Check;
  switch (type)
  {
  case VOLUME_FLOAT:
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, slice, texture->width, texture->height, 1, 
                    GL_LUMINANCE, GL_FLOAT, data);
    break;
  case VOLUME_BYTE:
  case VOLUME_BYTE_COMPRESSED:
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, slice, texture->width, texture->height, 1, 
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGB:
  case VOLUME_RGB_COMPRESSED:
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, slice, texture->width, texture->height, 1, 
                    GL_RGB, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGBA:
  case VOLUME_RGBA_COMPRESSED:
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, slice, texture->width, texture->height, 1, 
                    GL_RGBA, GL_UNSIGNED_BYTE, data);
    break;
  }
  GL_Error_Check;
}

std::string writeImage(GLubyte *image, int width, int height, const std::string& path, int channels)
{
  FilePath filepath(path);
  if (filepath.type == "png")
  {
    //Write data to image file
    std::ofstream file(filepath.full, std::ios::binary);
    //Requires y-flip as uses opposite y origin to OpenGL (except for libpng)
#ifndef HAVE_LIBPNG
    RawImageFlip(image, width, height, channels);
#endif
    write_png(file, channels, width, height, image);
  }
  else if (filepath.type == "jpeg" || filepath.type == "jpg")
  {
    //JPEG support with built in encoder
    // Fill in the compression parameter structure.
    //Requires y-flip as uses opposite y origin to OpenGL
    RawImageFlip(image, width, height, channels);

    jpge::params params;
    params.m_quality = 95;
    params.m_subsampling = jpge::H2V1;   //H2V2/H2V1/H1V1-none/0-grayscale
    if (!compress_image_to_jpeg_file(filepath.full.c_str(), width, height, channels, image, params))
    {
      fprintf(stderr, "[write_jpeg] File %s could not be saved\n", filepath.full.c_str());
      return "";
    }
  }
  else
  {
    std::string newpath = path + ".png";
    return writeImage(image, width, height, newpath, channels);
  }
  debug_print("[%s] File successfully written\n", filepath.full.c_str());
  return path;
}

std::string getImageString(GLubyte *image, int width, int height, int channels, bool jpeg)
{
  std::string encoded;
  if (!jpeg)
  {
#ifndef HAVE_LIBPNG
    //Requires y-flip as uses opposite y origin to OpenGL
    RawImageFlip(image, width, height, channels);
#endif
    // Write png to stringstream
    std::stringstream ss;
    write_png(ss, channels, width, height, image);
    //Base64 encode!
    std::string str = ss.str();
    encoded = "data:image/png;base64," + base64_encode(reinterpret_cast<const unsigned char*>(str.c_str()), str.length());
  }
  else
  {
    //Requires y-flip as uses opposite y origin to OpenGL
    RawImageFlip(image, width, height, channels);
    // Writes JPEG image to memory buffer.
    // On entry, jpeg_bytes is the size of the output buffer pointed at by jpeg, which should be at least ~1024 bytes.
    // If return value is true, jpeg_bytes will be set to the size of the compressed data.
    int jpeg_bytes = width * height * channels;
    unsigned char* jpeg = new unsigned char[jpeg_bytes];
    // Fill in the compression parameter structure.
    jpge::params params;
    params.m_quality = 95;
    params.m_subsampling = jpge::H1V1;   //H2V2/H2V1/H1V1-none/0-grayscale
    if (compress_image_to_jpeg_file_in_memory(jpeg, jpeg_bytes, width, height, channels, (const unsigned char *)image, params))
      debug_print("JPEG compressed, size %d\n", jpeg_bytes);
    else
      abort_program("JPEG compress error\n");
    //Base64 encode!
    encoded = "data:image/jpeg;base64," + base64_encode(reinterpret_cast<const unsigned char*>(jpeg), jpeg_bytes);
    delete[] jpeg;
  }

  return encoded;
}

#ifdef HAVE_LIBPNG
//PNG image read/write support
static void png_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
  std::istream* stream = (std::istream*)png_get_io_ptr(png_ptr);
  stream->read((char*)data, length);
  if (stream->fail() || stream->eof()) png_error(png_ptr, "Read Error");
}

static void png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
  std::ostream* stream = (std::ostream*)png_get_io_ptr(png_ptr);
  stream->write((const char*)data, length);
  if (stream->bad()) png_error(png_ptr, "Write Error");
}

static void png_flush(png_structp png_ptr)
{
  std::ostream* stream = (std::ostream*)png_get_io_ptr(png_ptr);
  stream->flush();
}

void* read_png(std::istream& stream, GLuint& channels, GLuint& width, GLuint& height)
{
  char header[8];   // 8 is the maximum size that can be checked
  unsigned int y;

  png_byte color_type;

  png_structp png_ptr;
  png_infop info_ptr;
  png_bytep * row_pointers;

  // open file and test for it being a png
  stream.read(header, 8);
  if (png_sig_cmp((png_byte*)&header, 0, 8))
    abort_program("[read_png_file] File is not recognized as a PNG file");

  // initialize stuff
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr)
    abort_program("[read_png_file] png_create_read_struct failed");

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    abort_program("[read_png_file] png_create_info_struct failed");

  //if (setjmp(png_jmpbuf(png_ptr)))
  //   abort_program("[read_png_file] Error during init_io");

  // initialize png I/O
  png_set_read_fn(png_ptr, (png_voidp)&stream, png_read_data);
  png_set_sig_bytes(png_ptr, 8);

  png_read_info(png_ptr, info_ptr);

  png_uint_32 imgWidth =  png_get_image_width(png_ptr, info_ptr);
  png_uint_32 imgHeight = png_get_image_height(png_ptr, info_ptr);
  //Number of channels
  channels   = png_get_channels(png_ptr, info_ptr);
  width = imgWidth;
  height = imgHeight;
  //Row bytes
  png_uint_32 rowbytes  = png_get_rowbytes(png_ptr, info_ptr);

  color_type = png_get_color_type(png_ptr, info_ptr);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
  {
    //Convert paletted to RGB
    png_set_palette_to_rgb(png_ptr);
    channels = 3;
  }

  debug_print("Reading PNG: %d x %d, colour type %d, channels %d\n", width, height, color_type, channels);

  png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);

  // read file
  //if (setjmp(png_jmpbuf(png_ptr)))
  //   abort_program("[read_png_file] Error during read_image");

  row_pointers = new png_bytep[height];
  //for (y=0; y<height; y++)
  //   row_pointers[y] = (png_byte*) malloc(rowbytes);
  png_bytep pixels = new png_byte[width * height * channels];
  for (y=0; y<height; y++)
    row_pointers[y] = (png_bytep)&pixels[rowbytes * y];

  png_read_image(png_ptr, row_pointers);

  png_destroy_read_struct(&png_ptr, &info_ptr,(png_infopp)0);
  png_destroy_info_struct(png_ptr, &info_ptr);

  delete[] row_pointers;

  return pixels;
}

void write_png(std::ostream& stream, int channels, int width, int height, void* data)
{
  int colour_type;
  png_bytep      pixels       = (png_bytep) data;
  int            rowStride;
  png_bytep*     row_pointers = new png_bytep[height];
  png_structp    pngWrite;
  png_infop      pngInfo;
  int            pixel_I;
  int            flip = 1;   //Flip input data vertically (for OpenGL framebuffer data)

  pngWrite = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!pngWrite)
  {
    fprintf(stderr, "[write_png_file] create PNG write struct failed");
    return;
  }

  //Setup for different write modes
  if (channels > 3)
  {
    colour_type = PNG_COLOR_TYPE_RGB_ALPHA;
    rowStride = width * 4;
  }
  else if (channels == 3)
  {
    colour_type = PNG_COLOR_TYPE_RGB;
    rowStride = width * 3;  // Don't need to pad lines! pack alignment is set to 1
  }
  else
  {
    colour_type = PNG_COLOR_TYPE_GRAY;
    rowStride = width;
  }

  // Set pointers to start of each scanline
  for ( pixel_I = 0 ; pixel_I < height ; pixel_I++ )
  {
    if (flip)
      row_pointers[pixel_I] = (png_bytep) &pixels[rowStride * (height - pixel_I - 1)];
    else
      row_pointers[pixel_I] = (png_bytep) &pixels[rowStride * pixel_I];
  }

  pngInfo = png_create_info_struct(pngWrite);
  if (!pngInfo)
  {
    fprintf(stderr, "[write_png_file] create PNG info struct failed");
    return;
  }
  //if (setjmp(png_jmpbuf(pngWrite)))
  //   abort_program("[write_png_file] Error during init_io");

  // initialize png I/O
  png_set_write_fn(pngWrite, (png_voidp)&stream, png_write_data, png_flush);
  png_set_compression_level(pngWrite, 6); //Much faster, not much larger

  //if (setjmp(png_jmpbuf(pngWrite)))
  //   abort_program("[write_png_file] Error writing header");

  png_set_IHDR(pngWrite, pngInfo,
               width, height,
               8,
               colour_type,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(pngWrite, pngInfo);
  png_write_image(pngWrite, row_pointers);
  png_write_end(pngWrite, pngInfo);

  // Clean Up
  png_destroy_info_struct(pngWrite, &pngInfo);
  png_destroy_write_struct(&pngWrite, NULL);
  delete[] row_pointers;
}

#else //HAVE_LIBPNG
void* read_png(std::istream& stream, GLuint& channels, GLuint& width, GLuint& height)
{
  //Read the stream
  std::string s(std::istreambuf_iterator<char>(stream), {});
  unsigned char* buffer = 0;
  channels = 4; //Always loads RGBA
  unsigned status = lodepng_decode32(&buffer, &width, &height, (const unsigned char*)s.c_str(), s.length());
  if (status != 0)
  {
    fprintf(stderr, "[read_png_file] decode failed");
    return NULL;
  }
  debug_print("Reading PNG: %d x %d, channels %d\n", width, height, channels);
  size_t size = width*height*channels;
  GLubyte* pixels = new GLubyte[size];
  memcpy(pixels, buffer, size);
  free(buffer);
  return pixels;
}

void write_png(std::ostream& stream, int channels, int width, int height, void* data)
{
  unsigned char* buffer;
  size_t buffersize;
  unsigned status = 0;
  if (channels == 3)
    status = lodepng_encode24(&buffer, &buffersize, (const unsigned char*)data, width, height);
  else if (channels == 4)
    status = lodepng_encode32(&buffer, &buffersize, (const unsigned char*)data, width, height);
  else
    abort_program("Invalid channels %d\n", channels);
  if (status != 0)
  {
    fprintf(stderr, "[write_png_file] encode failed");
    return;
  }
  debug_print("Writing PNG: %d x %d, channels %d\n", width, height, channels);
  stream.write((const char*)buffer, buffersize);
  free(buffer);
}

#endif //!HAVE_LIBPNG
