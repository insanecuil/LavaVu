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
//TimeStep - timestep data class
#ifndef TimeStep__
#define TimeStep__

#include "Geometry.h"

class TimeStep
{
public:
  int step;
  float time;
  std::string path;

  //Cached data
  std::vector<Geometry*> cache;

  TimeStep(int step, float time, const std::string& path="") : step(step), time(time), path(path) {}
  TimeStep() : step(0), time(0) {}

  ~TimeStep()
  {
    //Free cached geometry
    for (unsigned int i=0; i < cache.size(); i++)
      delete cache[i];
  }

  void write(std::vector<Geometry*> &data)
  {
    //for (unsigned int i=0; i < data.size(); i++)
    //   //Release any graphics memory
    //   data[i]->close();

    cache = data;

    //for (Geometry* g : cache)
    //  for (GeomData* d : g->geom)
    //    if (d->count)
    //      std::cout << "Cached [" << GeomData::names[g->type] << "] VERTICES: " << d->count << " VALUE ENTRIES " << d->values.size() << std::endl;
  }

  void read(std::vector<Geometry*> &data, bool clear=true)
  {
    //Disable clearing to cache all data on gpu too
    if (clear)
    {
      for (unsigned int i=0; i < data.size(); i++)
      {
        //Release any graphics memory
        data[i]->close();
      }
    }

    data = cache;

    //for (Geometry* g : cache)
    //  for (GeomData* d : g->geom)
    //    if (d->count)
    //      std::cout << "Cache Load [" << GeomData::names[g->type] << "] VERTICES: " << d->count << " VALUE ENTRIES " << d->values.size() << std::endl;
  }
};

#endif
