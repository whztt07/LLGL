/*
 * GLStateManager.cpp
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "GLStateManager.h"
#include "../GLRenderContext.h"
#include "../GLExtensions.h"
#include "../../../Core/Helper.h"


namespace LLGL
{


static const GLenum stateCapsMap[] =
{
    GL_BLEND,
    GL_COLOR_LOGIC_OP,
    GL_CULL_FACE,
    GL_DEBUG_OUTPUT,
    GL_DEBUG_OUTPUT_SYNCHRONOUS,
    GL_DEPTH_CLAMP,
    GL_DEPTH_TEST,
    GL_DITHER,
    GL_FRAMEBUFFER_SRGB,
    GL_LINE_SMOOTH,
    GL_MULTISAMPLE,
    GL_POLYGON_OFFSET_FILL,
    GL_POLYGON_OFFSET_LINE,
    GL_POLYGON_OFFSET_POINT,
    GL_POLYGON_SMOOTH,
    GL_PRIMITIVE_RESTART,
    GL_PRIMITIVE_RESTART_FIXED_INDEX,
    GL_RASTERIZER_DISCARD,
    GL_SAMPLE_ALPHA_TO_COVERAGE,
    GL_SAMPLE_ALPHA_TO_ONE,
    GL_SAMPLE_COVERAGE,
    GL_SAMPLE_SHADING,
    GL_SAMPLE_MASK,
    GL_SCISSOR_TEST,
    GL_STENCIL_TEST,
    GL_TEXTURE_CUBE_MAP_SEAMLESS,
    GL_PROGRAM_POINT_SIZE,
};

static const GLenum bufferTargetsMap[] =
{
    GL_ARRAY_BUFFER,
    GL_ATOMIC_COUNTER_BUFFER,
    GL_COPY_READ_BUFFER,
    GL_COPY_WRITE_BUFFER,
    GL_DISPATCH_INDIRECT_BUFFER,
    GL_DRAW_INDIRECT_BUFFER,
    GL_ELEMENT_ARRAY_BUFFER,
    GL_PIXEL_PACK_BUFFER,
    GL_PIXEL_UNPACK_BUFFER,
    GL_QUERY_BUFFER,
    GL_SHADER_STORAGE_BUFFER,
    GL_TEXTURE_BUFFER,
    GL_TRANSFORM_FEEDBACK_BUFFER,
    GL_UNIFORM_BUFFER,
};

static const GLenum textureTargetsMap[] =
{
    GL_TEXTURE_1D,
    GL_TEXTURE_2D,
    GL_TEXTURE_3D,
    GL_TEXTURE_1D_ARRAY,
    GL_TEXTURE_2D_ARRAY,
    GL_TEXTURE_RECTANGLE,
    GL_TEXTURE_CUBE_MAP,
    GL_TEXTURE_CUBE_MAP_ARRAY,
    GL_TEXTURE_BUFFER,
    GL_TEXTURE_2D_MULTISAMPLE,
    GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
};

static const GLenum textureLayersMap[] = 
{
    GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2, GL_TEXTURE3,
    GL_TEXTURE4, GL_TEXTURE5, GL_TEXTURE6, GL_TEXTURE7,
    GL_TEXTURE8, GL_TEXTURE9, GL_TEXTURE10, GL_TEXTURE11,
    GL_TEXTURE12, GL_TEXTURE13, GL_TEXTURE14, GL_TEXTURE15,
    GL_TEXTURE16, GL_TEXTURE17, GL_TEXTURE18, GL_TEXTURE19,
    GL_TEXTURE20, GL_TEXTURE21, GL_TEXTURE22, GL_TEXTURE23,
    GL_TEXTURE24, GL_TEXTURE25, GL_TEXTURE26, GL_TEXTURE27,
    GL_TEXTURE28, GL_TEXTURE29, GL_TEXTURE30, GL_TEXTURE31,
};


/* ----- Common ----- */

GLStateManager* GLStateManager::active = nullptr;

GLStateManager::GLStateManager()
{
    /* Initialize all states with zero */
    Fill(renderState_.values, false);
    Fill(bufferState_.boundBuffers, 0);

    for (auto& layer : textureState_.layers)
        Fill(layer.boundTextures, 0);

    activeTextureLayer_ = &(textureState_.layers[0]);

    /* Make this to the active state manager */
    GLStateManager::active = this;
}

void GLStateManager::MakeCurrentInfo(const GLRenderContext& renderContext)
{
    currentContextHeight_ = renderContext.GetContextHeight();
}

/* ----- Boolean states ----- */

void GLStateManager::Reset()
{
    /* Query all states from OpenGL */
    for (std::size_t i = 0; i < numStates; ++i)
        renderState_.values[i] = (glIsEnabled(stateCapsMap[i]) != GL_FALSE);
}

void GLStateManager::Set(GLState state, bool value)
{
    auto cap = static_cast<GLenum>(state);
    if (renderState_.values[cap] != value)
    {
        renderState_.values[cap] = value;
        if (value)
            glEnable(stateCapsMap[cap]);
        else
            glDisable(stateCapsMap[cap]);
    }
}

void GLStateManager::Enable(GLState state)
{
    auto cap = static_cast<GLenum>(state);
    if (!renderState_.values[cap])
    {
        renderState_.values[cap] = true;
        glEnable(stateCapsMap[cap]);
    }
}

void GLStateManager::Disable(GLState state)
{
    auto cap = static_cast<GLenum>(state);
    if (renderState_.values[cap])
    {
        renderState_.values[cap] = false;
        glDisable(stateCapsMap[cap]);
    }
}

bool GLStateManager::IsEnabled(GLState state) const
{
    return renderState_.values[static_cast<std::size_t>(state)];
}

void GLStateManager::PushState(GLState state)
{
    renderState_.valueStack.push(
        {
            state,
            renderState_.values[static_cast<std::size_t>(state)]
        }
    );
}

void GLStateManager::PopState()
{
    const auto& state = renderState_.valueStack.top();
    {
        Set(state.state, state.enabled);
    }
    renderState_.valueStack.pop();
}

void GLStateManager::PopStates(std::size_t count)
{
    while (count-- > 0)
        PopState();
}

/* ----- Common states ----- */

//private
void GLStateManager::AdjustViewport(GLViewport& viewport)
{
    viewport.y = static_cast<GLfloat>(currentContextHeight_) - viewport.height - viewport.y;
}

void GLStateManager::SetViewports(std::vector<GLViewport>& viewports)
{
    if (viewports.size() == 1)
    {
        auto& vp = viewports.front();

        if (emulateClipControl_)
            AdjustViewport(vp);

        glViewport(
            static_cast<GLint>(vp.x),
            static_cast<GLint>(vp.y),
            static_cast<GLsizei>(vp.width),
            static_cast<GLsizei>(vp.height)
        );
    }
    else if (viewports.size() > 1 && glViewportArrayv)
    {
        if (emulateClipControl_)
        {
            for (auto& vp : viewports)
                AdjustViewport(vp);
        }

        glViewportArrayv(
            0,
            static_cast<GLsizei>(viewports.size()),
            reinterpret_cast<const GLfloat*>(viewports.data())
        );
    }
}

void GLStateManager::SetDepthRanges(std::vector<GLDepthRange>& depthRanges)
{
    if (depthRanges.size() == 1)
    {
        const auto& dr = depthRanges.front();
        glDepthRange(dr.minDepth, dr.maxDepth);
    }
    else if (depthRanges.size() > 1 && glDepthRangeArrayv)
    {
        glDepthRangeArrayv(
            0,
            static_cast<GLsizei>(depthRanges.size()),
            reinterpret_cast<const GLdouble*>(depthRanges.data())
        );
    }
}

//private
void GLStateManager::AdjustScissor(GLScissor& scissor)
{
    scissor.y = currentContextHeight_ - scissor.height - scissor.y;
}

void GLStateManager::SetScissors(std::vector<GLScissor>& scissors)
{
    if (scissors.size() == 1)
    {
        auto& sc = scissors.front();

        if (emulateClipControl_)
            AdjustScissor(sc);

        glScissor(sc.x, sc.y, sc.width, sc.height);
    }
    else if (scissors.size() > 1 && glScissorArrayv)
    {
        if (emulateClipControl_)
        {
            for (auto& sc : scissors)
                AdjustScissor(sc);
        }

        glScissorArrayv(
            0,
            static_cast<GLsizei>(scissors.size()),
            reinterpret_cast<const GLint*>(scissors.data())
        );
    }
}

void GLStateManager::SetBlendStates(const std::vector<GLBlend>& blendStates, bool blendEnabled)
{
    if (blendStates.size() == 1)
    {
        /* Set blend state only for the single draw buffer */
        const auto& state = blendStates.front();

        if (commonState_.colorMask != state.colorMask)
        {
            commonState_.colorMask = state.colorMask;
            glColorMask(state.colorMask.r, state.colorMask.g, state.colorMask.b, state.colorMask.a);
        }

        if (blendEnabled)
            glBlendFuncSeparate(state.srcColor, state.destColor, state.srcAlpha, state.destAlpha);
    }
    else if (blendStates.size() > 1)
    {
        GLenum drawBuffer = GL_COLOR_ATTACHMENT0;

        /* Set respective blend state for each draw buffer */
        for (const auto& state : blendStates)
            SetBlendState(drawBuffer++, state, blendEnabled);
    }
}

void GLStateManager::SetBlendState(GLuint drawBuffer, const GLBlend& state, bool blendEnabled)
{
    if (glBlendFuncSeparatei != nullptr && glColorMaski != nullptr)
    {
        glColorMaski(drawBuffer, state.colorMask.r, state.colorMask.g, state.colorMask.b, state.colorMask.a);
        if (blendEnabled)
            glBlendFuncSeparatei(drawBuffer, state.srcColor, state.destColor, state.srcAlpha, state.destAlpha);
    }
    else
    {
        glDrawBuffer(drawBuffer);
        glColorMask(state.colorMask.r, state.colorMask.g, state.colorMask.b, state.colorMask.a);
        if (blendEnabled)
            glBlendFuncSeparate(state.srcColor, state.destColor, state.srcAlpha, state.destAlpha);
    }
}

void GLStateManager::SetClipControl(GLenum origin, GLenum depth)
{
    /*if (glClipControl)
        glClipControl(origin, depth);
    else*/
        emulateClipControl_ = (origin == GL_UPPER_LEFT);
}

void GLStateManager::SetDepthFunc(GLenum func)
{
    if (commonState_.depthFunc != func)
    {
        commonState_.depthFunc = func;
        glDepthFunc(func);
    }
}

void GLStateManager::SetStencilState(GLenum face, const GLStencil& state)
{
    switch (face)
    {
        case GL_FRONT:
            SetStencilState(GL_FRONT, commonState_.stencil[0], state);
            break;
        case GL_BACK:
            SetStencilState(GL_BACK, commonState_.stencil[1], state);
            break;
        case GL_FRONT_AND_BACK:
            SetStencilState(GL_FRONT, commonState_.stencil[0], state);
            SetStencilState(GL_BACK, commonState_.stencil[1], state);
            break;
    }
}

void GLStateManager::SetStencilState(GLenum face, GLStencil& to, const GLStencil& from)
{
    if (to.sfail != from.sfail || to.dpfail != from.dpfail || to.dppass != from.dppass)
    {
        to.sfail    = from.sfail;
        to.dpfail   = from.dpfail;
        to.dppass   = from.dppass;
        glStencilOpSeparate(face, to.sfail, to.dpfail, to.dppass);
    }

    if (to.func != from.func || to.ref != from.ref || to.mask != from.mask)
    {
        to.func = from.func;
        to.ref  = from.ref;
        to.mask = from.mask;
        glStencilFuncSeparate(face, to.func, to.ref, to.mask);
    }

    if (to.writeMask != from.writeMask)
    {
        to.writeMask = from.writeMask;
        glStencilMaskSeparate(face, to.writeMask);
    }
}

void GLStateManager::SetPolygonMode(GLenum mode)
{
    if (commonState_.polygonMode != mode)
    {
        commonState_.polygonMode = mode;
        glPolygonMode(GL_FRONT_AND_BACK, mode);
    }
}

void GLStateManager::SetCullFace(GLenum face)
{
    if (commonState_.cullFace != face)
    {
        commonState_.cullFace = face;
        glCullFace(face);
    }
}

void GLStateManager::SetFrontFace(GLenum mode)
{
    if (commonState_.frontFace != mode)
    {
        commonState_.frontFace = mode;
        glFrontFace(mode);
    }
}

void GLStateManager::SetDepthMask(GLboolean flag)
{
    if (commonState_.depthMask != flag)
    {
        commonState_.depthMask = flag;
        glDepthMask(flag);
    }
}

/* ----- Buffer binding ----- */

void GLStateManager::BindBuffer(GLBufferTarget target, GLuint buffer)
{
    /* Only bind buffer if the buffer changed */
    auto targetIdx = static_cast<std::size_t>(target);
    if (bufferState_.boundBuffers[targetIdx] != buffer)
    {
        bufferState_.boundBuffers[targetIdx] = buffer;
        glBindBuffer(bufferTargetsMap[targetIdx], buffer);
    }
}

void GLStateManager::BindBufferBase(GLBufferTarget target, GLuint index, GLuint buffer)
{
    /* Always bind buffer with a base index */
    auto targetIdx = static_cast<std::size_t>(target);
    bufferState_.boundBuffers[targetIdx] = buffer;
    glBindBufferBase(bufferTargetsMap[targetIdx], index, buffer);
}

void GLStateManager::BindVertexArray(GLuint buffer)
{
    /* Always bind vertex array */
    glBindVertexArray(buffer);
    bufferState_.boundBuffers[static_cast<std::size_t>(GLBufferTarget::ARRAY_BUFFER)] = 0;
    bufferState_.boundBuffers[static_cast<std::size_t>(GLBufferTarget::ELEMENT_ARRAY_BUFFER)] = 0;
}

void GLStateManager::ForcedBindBuffer(GLBufferTarget target, GLuint buffer)
{
    /* Always bind buffer with a forced binding */
    auto targetIdx = static_cast<std::size_t>(target);
    bufferState_.boundBuffers[targetIdx] = buffer;
    glBindBuffer(bufferTargetsMap[targetIdx], buffer);
}

void GLStateManager::PushBoundBuffer(GLBufferTarget target)
{
    bufferState_.boundBufferStack.push(
        {
            target,
            bufferState_.boundBuffers[static_cast<std::size_t>(target)]
        }
    );
}

void GLStateManager::PopBoundBuffer()
{
    const auto& state = bufferState_.boundBufferStack.top();
    {
        BindBuffer(state.target, state.buffer);
    }
    bufferState_.boundBufferStack.pop();
}

void GLStateManager::BindBuffer(const GLVertexBuffer& vertexBuffer)
{
    BindBuffer(GLBufferTarget::ARRAY_BUFFER, vertexBuffer.hwBuffer.GetID());
}

void GLStateManager::BindBuffer(const GLIndexBuffer& indexBuffer)
{
    BindBuffer(GLBufferTarget::ELEMENT_ARRAY_BUFFER, indexBuffer.hwBuffer.GetID());
}

void GLStateManager::BindBuffer(const GLConstantBuffer& constantBuffer)
{
    BindBuffer(GLBufferTarget::UNIFORM_BUFFER, constantBuffer.hwBuffer.GetID());
}

/* ----- Texture binding ----- */

void GLStateManager::ActiveTexture(unsigned int layer)
{
    if (textureState_.activeTexture != layer)
    {
        /* Active specified texture layer and store reference to bound textures array */
        textureState_.activeTexture = layer;
        activeTextureLayer_ = &(textureState_.layers[textureState_.activeTexture]);
        glActiveTexture(textureLayersMap[layer]);
    }
}

void GLStateManager::BindTexture(GLTextureTarget target, GLuint texture)
{
    /* Only bind texutre if the texutre changed */
    auto targetIdx = static_cast<std::size_t>(target);
    if (activeTextureLayer_->boundTextures[targetIdx] != texture)
    {
        activeTextureLayer_->boundTextures[targetIdx] = texture;
        glBindTexture(textureTargetsMap[targetIdx], texture);
    }
}

void GLStateManager::ForcedBindTexture(GLTextureTarget target, GLuint texture)
{
    /* Always bind texutre with a forced binding */
    auto targetIdx = static_cast<std::size_t>(target);
    activeTextureLayer_->boundTextures[targetIdx] = texture;
    glBindTexture(textureTargetsMap[targetIdx], texture);
}

void GLStateManager::PushBoundTexture(unsigned int layer, GLTextureTarget target)
{
    textureState_.boundTextureStack.push(
        {
            layer,
            target,
            (textureState_.layers[layer].boundTextures[static_cast<std::size_t>(target)])
        }
    );
}

void GLStateManager::PopBoundTexture()
{
    const auto& state = textureState_.boundTextureStack.top();
    {
        ActiveTexture(state.layer);
        BindTexture(state.target, state.texture);
    }
    textureState_.boundTextureStack.pop();
}

static GLTextureTarget GetTextureTarget(const TextureType type)
{
    switch (type)
    {
        case TextureType::Texture1D:        return GLTextureTarget::TEXTURE_1D;
        case TextureType::Texture2D:        return GLTextureTarget::TEXTURE_2D;
        case TextureType::Texture3D:        return GLTextureTarget::TEXTURE_3D;
        case TextureType::TextureCube:      return GLTextureTarget::TEXTURE_CUBE_MAP;
        case TextureType::Texture1DArray:   return GLTextureTarget::TEXTURE_1D_ARRAY;
        case TextureType::Texture2DArray:   return GLTextureTarget::TEXTURE_2D_ARRAY;
        case TextureType::TextureCubeArray: return GLTextureTarget::TEXTURE_CUBE_MAP_ARRAY;
        default:                            break;
    }
    throw std::invalid_argument("failed to convert texture type to OpenGL texture target");
}

void GLStateManager::BindTexture(const GLTexture& texture)
{
    BindTexture(GetTextureTarget(texture.GetType()), texture.GetID());
}

void GLStateManager::ForcedBindTexture(const GLTexture& texture)
{
    ForcedBindTexture(GetTextureTarget(texture.GetType()), texture.GetID());
}

/* ----- Shader binding ----- */

void GLStateManager::BindShaderProgram(GLuint program)
{
    if (shaderState_.boundProgram != program)
    {
        shaderState_.boundProgram = program;
        glUseProgram(program);
    }
}

void GLStateManager::PushShaderProgram()
{
    shaderState_.boundProgramStack.push(shaderState_.boundProgram);
}

void GLStateManager::PopShaderProgram()
{
    BindShaderProgram(shaderState_.boundProgramStack.top());
    shaderState_.boundProgramStack.pop();
}


} // /namespace LLGL



// ================================================================================