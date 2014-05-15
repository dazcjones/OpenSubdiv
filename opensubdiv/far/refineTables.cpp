//
//   Copyright 2014 DreamWorks Animation LLC.
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
#include "../far/refineTables.h"

#include <cassert>


namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {


//
//  Relatively trivial construction/destruction -- the base level (level[0]) needs
//  to be explicitly initialized after construction and refinement then applied
//
FarRefineTables::FarRefineTables(SdcType schemeType, SdcOptions schemeOptions) :
    _subdivType(schemeType),
    _subdivOptions(schemeOptions),
    _isUniform(true),
    _maxLevel(0),
    _levels(1)
{
}

FarRefineTables::~FarRefineTables()
{
}

void
FarRefineTables::Unrefine()
{
    if (_levels.size()) {
        _levels.resize(1);
    }
    _refinements.clear();
}

void
FarRefineTables::Clear()
{
    _levels.clear();
    _refinements.clear();
}


//
//  Accessors to the topology information:
//
int
FarRefineTables::GetVertCount() const
{
    int sum = 0;
    for (int i = 0; i < (int)_levels.size(); ++i) {
        sum += _levels[i].vertCount();
    }
    return sum;
}
int
FarRefineTables::GetEdgeCount() const
{
    int sum = 0;
    for (int i = 0; i < (int)_levels.size(); ++i) {
        sum += _levels[i].edgeCount();
    }
    return sum;
}
int
FarRefineTables::GetFaceCount() const
{
    int sum = 0;
    for (int i = 0; i < (int)_levels.size(); ++i) {
        sum += _levels[i].faceCount();
    }
    return sum;
}

//
//  Main refinement method -- allocating and initializing levels and refinements:
//
void
FarRefineTables::RefineUniform(int maxLevel, bool fullTopology, bool computeMasks)
{
    assert(_levels[0].vertCount() > 0);  //  Make sure the base level has been initialized
    assert(_subdivType == TYPE_CATMARK);

    //
    //  Allocate the stack of levels and the refinements between them:
    //
    _isUniform = true;
    _maxLevel = maxLevel;

    _levels.resize(maxLevel + 1);
    _refinements.resize(maxLevel);

    //
    //  Initialize refinement options for Vtr -- adjusting full-topology for the last level:
    //
    VtrRefinement::Options refineOptions;
    refineOptions._sparse       = false;
    refineOptions._computeMasks = computeMasks;

    for (int i = 1; i <= maxLevel; ++i) {
        refineOptions._faceTopologyOnly = fullTopology ? false : (i == maxLevel);

        _refinements[i-1].initialize(_levels[i-1], _levels[i]);
        _refinements[i-1].refine(refineOptions);
    }
}


void
FarRefineTables::RefineAdaptive(int subdivLevel, bool fullTopology, bool computeMasks)
{
    assert(_levels[0].vertCount() > 0);  //  Make sure the base level has been initialized
    assert(_subdivType == TYPE_CATMARK);

    //
    //  Allocate the stack of levels and the refinements between them:
    //
    _isUniform = false;
    _maxLevel = subdivLevel;

    //  Should we presize all or grow one at a time as needed?
    _levels.resize(subdivLevel + 1);
    _refinements.resize(subdivLevel);

    //
    //  Initialize refinement options for Vtr:
    //
    //  Enabling both parent and child tagging for now
    bool parentTagging = true;
    bool childTagging  = true;

    VtrRefinement::Options refineOptions;
    refineOptions._sparse           = true;
    refineOptions._faceTopologyOnly = !fullTopology;
    refineOptions._computeMasks     = computeMasks;
    refineOptions._parentTagging    = parentTagging;
    refineOptions._childTagging     = childTagging;

    for (int i = 1; i <= subdivLevel; ++i) {
        //  Keeping full topology on for debugging -- may need to go back a level and "prune"
        //  its topology if we don't use the full depth
        refineOptions._faceTopologyOnly = false;

        VtrLevel& parentLevel     = _levels[i-1];
        VtrLevel& childLevel      = _levels[i];
        VtrRefinement& refinement = _refinements[i-1];

        refinement.initialize(parentLevel, childLevel);

        //
        //  Initialize a Selector to mark a sparse set of components for refinement.  The
        //  previous refinement may include tags on its child components that are relevant,
        //  which is why the Selector identifies it.
        //
        //  It's ebatable whether our begin/end should be moved into the feature adaptive code
        //  that uses the Selector -- or the use of the Selector entirely for that matter...
        //
        VtrSparseSelector selector(refinement);
        selector.setPreviousRefinement((i-1) ? &_refinements[i-2] : 0);

        selector.beginSelection(parentTagging);

        catmarkFeatureAdaptiveSelector(selector);

        selector.endSelection();

        //
        //  Continue refining if something selected, otherwise terminate refinement and trim
        //  the Level and Refinement vectors to remove the curent refinement and child that
        //  were in progress:
        //
        if (!selector.isSelectionEmpty()) {
            refinement.refine(refineOptions);

            //childLevel.print(&refinement);
            //assert(childLevel.validateTopology());
        } else {
            //  Note that if we support the "full topology at last level" option properly,
            //  we should prune the previous level generated, as it is now the last...
            int maxLevel = i - 1;

            _maxLevel = maxLevel;
            _levels.resize(maxLevel + 1);
            _refinements.resize(maxLevel);
            break;
        }
    }
}

//
//   Below is a prototype of a method to select features for sparse refinement at each level.
//   It assumes we have a freshly initialized VtrSparseSelector (i.e. nothing already selected)
//   and will select all relevant topological features for inclusion in the subsequent sparse
//   refinement.
//
//   A couple general points on "feature adaptive selection" in general...
//
//   1)  With appropriate topological tags on the components, i.e. which vertices are
//       extra-ordinary, non-manifold, etc., there's no reason why this can't be written
//       in a way that is independent of the subdivision scheme.  All of the creasing
//       cases are independent, leaving only the regularity associated with the scheme.
//
//   2)  Since feature adaptive refinement is all about the generation of patches, it is
//       inherently more concerned with the topology of faces than of vertices or edges.
//       In order to fully exploit the generation of regular patches in the presence of
//       infinitely sharp edges, we need to consider the face as a whole and not trigger
//       refinement based on a vertex, e.g. an extra-ordinary vertex may be present, but
//       with all infinitely sharp edges around it, every patch is potentially a regular
//       corner.  It is currently difficult to extract all that is needed from the edges
//       and vertices of a face, but once more tags are added to the edges and vertices,
//       this can be greatly simplified.
//       
//  So once more tagging of components is in place, I favor a more face-centric approach than
//  what exists below.  We should be able to iterate through the faces once and make optimal
//  decisions without any additional passes through the vertices or edges here.  Most common
//  cases will be readily detected, i.e. smooth regular patches or those with any semi-sharp
//  feature, leaving only those with a mixture of smooth and infinitely sharp features for
//  closer analysis.
//
//  Given that we cannot avoid the need to traverse the face list for level 0 in order to
//  identify irregular faces for subdivision, we will hopefully only have to visit N faces
//  and skip the additional traversal of the N vertices and 2*N edges present here.  The
//  argument against the face-centric approach is that shared vertices and edges are
//  inspected multiple times, but with relevant data stored in tags in these components,
//  that work should be minimal.
//
void
FarRefineTables::catmarkFeatureAdaptiveSelector(VtrSparseSelector& selector)
{
    VtrLevel const& level = selector.getRefinement().parent();

    //
    //  For faces, we only need to select irregular faces from level 0 -- which will
    //  generate an extra-ordinary vertex in its interior:
    //
    //  Not so fast...
    //      According to far/meshFactory.h, we must also account for the following cases:
    //
    //  "Quad-faces with 2 non-consecutive boundaries need to be flagged for refinement as
    //  boundary patches."
    //
    //       o ........ o ........ o ........ o
    //       .          |          |          .     ... boundary edge
    //       .          |   needs  |          .
    //       .          |   flag   |          .     --- regular edge
    //       .          |          |          .
    //       o ........ o ........ o ........ o
    //
    //  ... presumably because this type of "incomplete" B-spline patch is not supported by
    //  the set of patch types in FarPatchTables (though it is regular).
    //
    //  And additionally we must isolate sharp corners if they are on a face with any
    //  more boundary edges (than the two defining the corner).  So in the above diagram,
    //  if all corners are sharp, then all three faces need to be subdivided, but only
    //  the one level.
    //
    //  Fortunately this only needs to be tested at level 0 too -- its analogous to the
    //  isolation required of extra-ordinary patches, required here for regular patches
    //  since only a specific set of B-spline boundary patches is supported.
    //
    //  Arguably, for the sharp corner case, we can deal with that during the vertex
    //  traversal, but it requires knowledge of a greater topological neighborhood than
    //  the vertex itself -- knowledge we have when detecting the opposite boundary case
    //  and so might as well detect here.  Whether the corner is sharp or not is irrelevant
    //  as both the extraordinary smooth, or the regular sharp cases need isolation.
    //
    if (level.depth() == 0) {
        for (VtrIndex face = 0; face < level.faceCount(); ++face) {
            VtrIndexArray const faceVerts = level.accessFaceVerts(face);

            if (faceVerts.size() != 4) {
                selector.selectFace(face);
            } else {
                VtrIndexArray const faceEdges = level.accessFaceEdges(face);

                int boundaryEdgeSum = (level.accessEdgeFaces(faceEdges[0]).size() == 1) +
                                      (level.accessEdgeFaces(faceEdges[1]).size() == 1) +
                                      (level.accessEdgeFaces(faceEdges[2]).size() == 1) +
                                      (level.accessEdgeFaces(faceEdges[3]).size() == 1);
                if ((boundaryEdgeSum > 2) || ((boundaryEdgeSum == 2) &&
                    (level.accessEdgeFaces(faceEdges[0]).size() == level.accessEdgeFaces(faceEdges[2]).size()))) {
                    selector.selectFace(face);
                }
            }
        }
    }

    //
    //  For vertices, we want to immediatly skip neighboring vertices generated from the
    //  previous level (the percentage will typically be high enough to warrant immediate
    //  culling, as the will include all perimeter vertices).
    //
    //  Sharp vertices are complicated by the corner case -- an infinitely sharp corner is
    //  considered a regular feature and not sharp, but a corner with any other sharpness
    //  will eventually become extraordinary once its sharpness has decayed -- so it is
    //  both sharp and irregular.
    //
    //  For the remaining topological cases, non-manifold vertices should be considered
    //  along with extra-ordinary, and we should be testing a vertex tag for thats (and
    //  maybe the extra-ordinary too)
    //
    for (VtrIndex vert = 0; vert < level.vertCount(); ++vert) {
        if (selector.isVertexIncomplete(vert)) continue;

        bool selectVertex = false;

        float vertSharpness = level.vertSharpness(vert);
        if (vertSharpness > 0.0) {
            selectVertex = (level.accessVertFaces(vert).size() != 1) || (vertSharpness < SdcCrease::INFINITE);
        } else {
            VtrIndexArray const vertFaces = level.accessVertFaces(vert);
            VtrIndexArray const vertEdges = level.accessVertEdges(vert);

            //  Should be non-manifold test -- remaining cases assume manifold...
            if (vertFaces.size() == vertEdges.size()) {
                selectVertex = (vertFaces.size() != 4);
            } else {
                selectVertex = (vertFaces.size() != 2);
            }
        }
        if (selectVertex) {
            selector.selectVertexFaces(vert);
        }
    }

    //
    //  For edges, we only care about sharp edges, so we can immediately skip all smooth.
    //
    //  That leaves us dealing with sharp edges that may in the interior or on a boundary.
    //  A boundary edge is always a (regular) B-spline boundary, unless something at an end
    //  vertex makes it otherwise.  But any end vertex that would make the edge irregular
    //  should already have been detected above.  So I'm pretty sure we can just skip all
    //  boundary edges.
    //
    //  So reject boundaries, but in a way that includes non-manifold edges for selection.
    //
    //  And as for vertices, skip incomplete neighboring vertices from the previous level.
    //
    for (VtrIndex edge = 0; edge < level.edgeCount(); ++edge) {
        if ((level.edgeSharpness(edge) <= 0.0) || (level.accessEdgeFaces(edge).size() < 2)) continue;

        VtrIndexArray const edgeVerts = level.accessEdgeVerts(edge);
        if (!selector.isVertexIncomplete(edgeVerts[0])) {
            selector.selectVertexFaces(edgeVerts[0]);
        }
        if (!selector.isVertexIncomplete(edgeVerts[1])) {
            selector.selectVertexFaces(edgeVerts[1]);
        }
    }
}

} // end namespace OPENSUBDIV_VERSION
} // end namespace OpenSubdiv
