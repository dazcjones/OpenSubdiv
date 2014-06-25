//
//   Copyright 2013 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//


//------------------------------------------------------------------------------
// Tutorial description:
//
//

#include <far/refineTablesFactory.h>
#include <far/stencilTables.h>
#include <far/stencilTablesFactory.h>

#include <cstdio>
#include <cstring>

//------------------------------------------------------------------------------
// Vertex container implementation.
//
struct Vertex {

    // Hbr minimal required interface ----------------------
    Vertex() { }

    Vertex(int /*i*/) { }

    Vertex(Vertex const & src) {
        _position[0] = src._position[0];
        _position[1] = src._position[1];
        _position[1] = src._position[1];
    }

    void Clear( void * =0 ) {
        _position[0]=_position[1]=_position[2]=0.0f;
    }

    void AddWithWeight(Vertex const & src, float weight) {
        _position[0]+=weight*src._position[0];
        _position[1]+=weight*src._position[1];
        _position[2]+=weight*src._position[2];
    }

    void AddVaryingWithWeight(Vertex const &, float) { }

    // Public interface ------------------------------------
    void SetPosition(float x, float y, float z) {
        _position[0]=x;
        _position[1]=y;
        _position[2]=z;
    }

    float const * GetPosition() const {
        return _position;
    }

private:
    float _position[3];
};

//------------------------------------------------------------------------------
// Pyramid geometry from catmark_pyramid.h

static float g_verts[24] = {-0.5f, -0.5f,  0.5f,
                             0.5f, -0.5f,  0.5f,    
                            -0.5f,  0.5f,  0.5f,    
                             0.5f,  0.5f,  0.5f,    
                            -0.5f,  0.5f, -0.5f,    
                             0.5f,  0.5f, -0.5f,    
                            -0.5f, -0.5f, -0.5f,    
                             0.5f, -0.5f, -0.5f };  

static int g_nverts = 8,
           g_nfaces = 6;

static int g_vertsperface[6] = { 4, 4, 4, 4, 4, 4 };

static int g_vertIndices[24] = { 0, 1, 3, 2,
                                 2, 3, 5, 4,
                                 4, 5, 7, 6,
                                 6, 7, 1, 0,
                                 1, 7, 5, 3,
                                 6, 0, 2, 4  };

using namespace OpenSubdiv;

static FarRefineTables * createRefineTables();

//------------------------------------------------------------------------------
int main(int, char **) {


    FarRefineTables * refTables = createRefineTables();

    int maxlevel = 1;

    // Uniformly refine the topolgy up to 'maxlevel'
    refTables->RefineUniform( maxlevel );

    // Use the factory to create discrete stencil tables
    FarStencilTables const * stencilTable =
        FarStencilTablesFactory::Create(*refTables);
    
    int nstencils = stencilTable->GetNumStencils();

    // Allocate vertex primvar buffer
    std::vector<Vertex> vertexBuffer(nstencils);
    
    Vertex * controlValues = reinterpret_cast<Vertex *>(g_verts);
    
    // Apply stencils to control vertex data. Our primvar data stride is 3
    // since our Vertex class only interpolates 3-axis position data.
    stencilTable->UpdateValues(controlValues, &vertexBuffer[0], 3);

    // Print MEL script with particles at the location of the vertices
    printf("particle ");
    for (int i=0; i<(int)vertexBuffer.size(); ++i) {
        float const * pos = vertexBuffer[i].GetPosition();
        printf("-p %f %f %f\n", pos[0], pos[1], pos[2]);
    }
    printf("-c 1;\n");
}

//------------------------------------------------------------------------------
static FarRefineTables * 
createRefineTables() {

    // Populate a topology descriptor with our raw data
    typedef FarRefineTablesFactoryBase::TopologyDescriptor Descriptor;

    SdcType type = OpenSubdiv::TYPE_CATMARK;

    SdcOptions options;
    options.SetVVarBoundaryInterpolation(SdcOptions::VVAR_BOUNDARY_EDGE_ONLY);

    Descriptor desc;
    desc.numVertices  = g_nverts;
    desc.numFaces     = g_nfaces;
    desc.vertsPerFace = g_vertsperface;
    desc.vertIndices  = g_vertIndices;


    // Instantiate a FarRefineTables from the descriptor
    return FarRefineTablesFactory<Descriptor>::Create(type, options, desc);

}

//------------------------------------------------------------------------------
