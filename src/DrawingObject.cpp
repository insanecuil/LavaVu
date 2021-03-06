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

#include "DrawingObject.h"
#include "Model.h"

DrawingObject::DrawingObject(DrawState& drawstate, std::string name, std::string props, unsigned int id)
  : drawstate(drawstate), properties(drawstate.globals, drawstate.defaults), dbid(id)
{
  texture = NULL;
  skip = drawstate.global("noload");
  //Name set in properties overrides that passed from database
  properties.data["name"] = name;

  //Fix any names with spaces
  std::replace(name.begin(), name.end(), ' ', '_');

  properties.parseSet(props);

  //All props now lowercase, fix a couple of legacy camelcase values
  if (properties.has("pointSize")) {properties.data["pointsize"] = properties["pointSize"]; properties.data.erase("pointSize");}

  properties.data["visible"] = true;
  colourIdx = 0; //Default colouring data is first value block
  opacityIdx = 1;
  colourMap = opacityMap = NULL;
  setup();
}

DrawingObject::~DrawingObject()
{
  if (texture)
    delete texture;
}

void DrawingObject::setup()
{
  //Cache values for faster lookups during draw calls
  colour = properties["colour"];
  opacity = 1.0;
  if (properties.has("opacity"))
    opacity = properties["opacity"];
  //Convert values (1,255] -> [0,1]
  if (opacity > 1.0) opacity /= 255.0;
  //Cache colourmaps
  colourMap = opacityMap = NULL;
  if (drawstate.colourMaps)
  {
    int cmapid = properties["colourmap"];
    if (cmapid >= 0 && cmapid < drawstate.colourMaps->size()) colourMap = (*drawstate.colourMaps)[cmapid];
    int omapid = properties["opacitymap"];
    if (omapid >= 0 && omapid < drawstate.colourMaps->size()) opacityMap = (*drawstate.colourMaps)[omapid];
  }
}

TextureData* DrawingObject::useTexture(ImageLoader* tex)
{
  GL_Error_Check;
  //Use/load default texture
  if (!tex)
  {
    if (texture)
      tex = texture;
    else if (properties.has("texturefile"))
    {
      std::string texfn = properties["texturefile"];
      if (texfn.length() > 0 && FileExists(texfn))
      {
        tex = texture = new ImageLoader(texfn);
      }
      else
      {
        if (texfn.length() > 0) debug_print("Texture File: %s not found!\n", texfn.c_str());
        //If load failed, skip from now on
        properties.data["texturefile"] = "";
      }
    }
  }

  if (tex)
  {
    //On first call only loads data from external file if provided
    //Then, and on subsequent calls, simply returns the preloaded texture
    return tex->use();
  }

  //No texture:
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_TEXTURE_3D);
  return NULL;
}

