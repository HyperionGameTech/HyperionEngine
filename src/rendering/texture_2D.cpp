#include "texture_2D.h"
#include "../core_engine.h"
#include "../gl_util.h"
#include "../util.h"
#include <cassert>

namespace hyperion {

Texture2D::Texture2D()
    : Texture(TextureType::TEXTURE_TYPE_2D)
{
}

Texture2D::Texture2D(int width, int height, unsigned char *bytes)
    : Texture(TextureType::TEXTURE_TYPE_2D, width, height, bytes)
{
}

Texture2D::~Texture2D()
{
    // deleted in parent destructor
}

void Texture2D::UploadGpuData(bool should_upload_data)
{
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T, wrap_t);

    if (should_upload_data) {
        glTexImage2D(GL_TEXTURE_2D, 0, ifmt,
            width, height, 0, fmt, GL_UNSIGNED_BYTE, bytes);

        CatchGLErrors("glTexImage2D failed.", false);

        if (min_filter == GL_LINEAR_MIPMAP_LINEAR ||
            min_filter == GL_LINEAR_MIPMAP_NEAREST ||
            min_filter == GL_NEAREST_MIPMAP_NEAREST) {
            glGenerateMipmap(GL_TEXTURE_2D);
            CatchGLErrors("Failed to generate Texture2D mipmaps.", false);
        }
    }
}

void Texture2D::CopyData(Texture * const other)
{
    AssertThrow(other != nullptr);
    AssertThrow(width == other->GetWidth());
    AssertThrow(height == other->GetHeight());
    AssertThrow(ifmt == other->GetInternalFormat());
    AssertThrow(fmt == other->GetFormat());

    glCopyTexImage2D(GL_TEXTURE_2D, 0, fmt, 0, 0, width, height, 0);

    CatchGLErrors("Failed to copy texture data", false);
}

void Texture2D::Use()
{
    glBindTexture(GL_TEXTURE_2D, id);
}

void Texture2D::End()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace hyperion
