#include "framebuffer.h"
#include "../opengl.h"
#include "../core_engine.h"

#include <iostream>

namespace hyperion {

decltype(Framebuffer::default_texture_attributes)
Framebuffer::default_texture_attributes = {
    Framebuffer::FramebufferTextureAttributes( // color
        "ColorMap",
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        true
    ),
    Framebuffer::FramebufferTextureAttributes( // normals
        "NormalMap",
        CoreEngine::GLEnums::RGB,
        CoreEngine::GLEnums::RGBA32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),
    Framebuffer::FramebufferTextureAttributes( // positions
        "PositionMap",
        CoreEngine::GLEnums::RGB,
        CoreEngine::GLEnums::RGBA32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),
    Framebuffer::FramebufferTextureAttributes( // userdata
        "DataMap",
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA8,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),
    Framebuffer::FramebufferTextureAttributes( // ssao / gi
        "SSLightingMap",
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA8,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        true
    ),
    Framebuffer::FramebufferTextureAttributes( // tangents
        "TangentMap",
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA8,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),
    Framebuffer::FramebufferTextureAttributes( // bitangents
        "BitangentMap",
        CoreEngine::GLEnums::RGBA,
        CoreEngine::GLEnums::RGBA8,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    ),

    // *** depth must remain last
    Framebuffer::FramebufferTextureAttributes( // depth
        "DepthMap",
        CoreEngine::GLEnums::DEPTH_COMPONENT,
        CoreEngine::GLEnums::DEPTH_COMPONENT32F,
        CoreEngine::GLEnums::NEAREST,
        CoreEngine::GLEnums::NEAREST,
        false
    )
};

Framebuffer::Framebuffer(int width, int height)
    : width(width),
      height(height)
{
    is_uploaded = false;
    is_created = false;
}

Framebuffer::~Framebuffer()
{
    if (is_created) {
        glDeleteFramebuffers(1, &id);
    }

    is_uploaded = false;
    is_created = false;
}

void Framebuffer::End()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace hyperion
