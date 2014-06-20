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

#ifndef OSD_GLSL_TRANSFORM_FEEDBACK_COMPUTE_CONTROLLER_H
#define OSD_GLSL_TRANSFORM_FEEDBACK_COMPUTE_CONTROLLER_H

#include "../version.h"

#include "../far/dispatcher.h"
#include "../osd/glslTransformFeedbackComputeContext.h"
#include "../osd/vertexDescriptor.h"

#include <vector>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

class OsdGLSLTransformFeedbackKernelBundle;

/// \brief Compute controller for launching GLSLTransformFeedback transform feedback
/// subdivision kernels.
///
/// OsdGLSLTransformFeedbackComputeController is a compute controller class to launch
/// GLSLTransformFeedback transfrom feedback subdivision kernels. It requires
/// OsdGLVertexBufferInterface as arguments of Refine function.
///
/// Controller entities execute requests from Context instances that they share
/// common interfaces with. Controllers are attached to discrete compute devices
/// and share the devices resources with Context entities.
///
class OsdGLSLTransformFeedbackComputeController {
public:
    typedef OsdGLSLTransformFeedbackComputeContext ComputeContext;

    /// Constructor.
    OsdGLSLTransformFeedbackComputeController();

    /// Destructor.
    ~OsdGLSLTransformFeedbackComputeController();

    /// Launch subdivision kernels and apply to given vertex buffers.
    ///
    /// @param  context       the OsdCpuContext to apply refinement operations to
    ///
    /// @param  batches       vector of batches of vertices organized by operative 
    ///                       kernel
    ///
    /// @param  vertexBuffer  vertex-interpolated data buffer
    ///
    /// @param  varyingBuffer varying-interpolated data buffer
    ///
    /// @param  vertexDesc    the descriptor of vertex elements to be refined.
    ///                       if it's null, all primvars in the vertex buffer
    ///                       will be refined.
    ///
    /// @param  varyingDesc   the descriptor of varying elements to be refined.
    ///                       if it's null, all primvars in the varying buffer
    ///                       will be refined.
    ///
    template<class VERTEX_BUFFER, class VARYING_BUFFER>
    void Refine(OsdGLSLTransformFeedbackComputeContext const *context,
                FarKernelBatchVector const &batches,
                VERTEX_BUFFER *vertexBuffer,
                VARYING_BUFFER *varyingBuffer,
                OsdVertexBufferDescriptor const *vertexDesc=NULL,
                OsdVertexBufferDescriptor const *varyingDesc=NULL) {

        if (batches.empty()) return;

        bind(vertexBuffer, varyingBuffer, vertexDesc, varyingDesc);
        context->BindTableTextures(_currentBindState.kernelBundle);

        FarDispatcher::Refine(this, context, batches, /*maxlevel*/-1);

        context->UnbindTableTextures();
        unbind();
    }

    /// Launch subdivision kernels and apply to given vertex buffers.
    ///
    /// @param  context       the OsdCpuContext to apply refinement operations to
    ///
    /// @param  batches       vector of batches of vertices organized by operative 
    ///                       kernel
    ///
    /// @param  vertexBuffer  vertex-interpolated data buffer
    ///
    template<class VERTEX_BUFFER>
    void Refine(OsdGLSLTransformFeedbackComputeContext const *context,
                FarKernelBatchVector const &batches,
                VERTEX_BUFFER *vertexBuffer) {
        Refine(context, batches, vertexBuffer, (VERTEX_BUFFER*)NULL);
    }

    /// Waits until all running subdivision kernels finish.
    void Synchronize();

protected:
    friend class FarDispatcher;
    void ApplyBilinearFaceVerticesKernel(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyBilinearEdgeVerticesKernel(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyBilinearVertexVerticesKernel(FarKernelBatch const &batch, ComputeContext const *context) const;


    void ApplyCatmarkFaceVerticesKernel(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyCatmarkQuadFaceVerticesKernel(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyCatmarkTriQuadFaceVerticesKernel(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyCatmarkEdgeVerticesKernel(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyCatmarkVertexVerticesKernelB(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyCatmarkVertexVerticesKernelA1(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyCatmarkVertexVerticesKernelA2(FarKernelBatch const &batch, ComputeContext const *context) const;


    void ApplyLoopEdgeVerticesKernel(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyLoopVertexVerticesKernelB(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyLoopVertexVerticesKernelA1(FarKernelBatch const &batch, ComputeContext const *context) const;

    void ApplyLoopVertexVerticesKernelA2(FarKernelBatch const &batch, ComputeContext const *context) const;


    void ApplyVertexEdits(FarKernelBatch const &batch, ComputeContext const *context) const;

    OsdGLSLTransformFeedbackKernelBundle * getKernels(
        OsdVertexBufferDescriptor const &vertexDesc,
        OsdVertexBufferDescriptor const &varyingDesc,
        bool interleaved);

    void bindResources();

    void unbindResources();

    template<class VERTEX_BUFFER, class VARYING_BUFFER>
    void bind(VERTEX_BUFFER *vertex, VARYING_BUFFER *varying,
              OsdVertexBufferDescriptor const *vertexDesc,
              OsdVertexBufferDescriptor const *varyingDesc) {

        // if the vertex buffer descriptor is specified, use it.
        // otherwise, assumes the data is tightly packed in the vertex buffer.
        if (vertexDesc) {
            _currentBindState.vertexDesc = *vertexDesc;
        } else {
            int numElements = vertex ? vertex->GetNumElements() : 0;
            _currentBindState.vertexDesc = OsdVertexBufferDescriptor(
                0, numElements, numElements);
        }
        if (varyingDesc) {
            _currentBindState.varyingDesc = *varyingDesc;
        } else {
            int numElements = varying ? varying->GetNumElements() : 0;
            _currentBindState.varyingDesc = OsdVertexBufferDescriptor(
                0, numElements, numElements);
        }

        bool interleaved = (vertex and varying and (vertex == varying));
        _currentBindState.vertexBuffer = vertex ? vertex->BindVBO() : 0;
        _currentBindState.varyingBuffer = varying ? varying->BindVBO() : 0;
        _currentBindState.kernelBundle = getKernels(_currentBindState.vertexDesc,
                                                    _currentBindState.varyingDesc,
                                                    interleaved);

        bindResources();
    }

    /// Unbinds any previously bound vertex and varying data buffers.
    void unbind() {
        _currentBindState.Reset();

        unbindResources();
    }

private:
    struct BindState {
        BindState() : vertexBuffer(0), varyingBuffer(0), kernelBundle(NULL) {}
        void Reset() {
            vertexBuffer = varyingBuffer = 0;
            vertexDesc.Reset();
            varyingDesc.Reset();
        }
        GLuint vertexBuffer;
        GLuint varyingBuffer;
        OsdVertexBufferDescriptor vertexDesc;
        OsdVertexBufferDescriptor varyingDesc;
        OsdGLSLTransformFeedbackKernelBundle *kernelBundle;
    };
    BindState _currentBindState;

    std::vector<OsdGLSLTransformFeedbackKernelBundle *> _kernelRegistry;
    GLuint _vertexTexture, _varyingTexture;
    GLuint _vao;
};

}  // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;

}  // end namespace OpenSubdiv

#endif  // OSD_GLSL_TRANSFORM_FEEDBACK_COMPUTE_CONTROLLER_H
