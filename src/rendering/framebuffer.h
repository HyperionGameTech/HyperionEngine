#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "texture_2D.h"
#include "../util.h"

#include <memory>
#include <array>
#include <cmath>

#define FRAMEBUFFER_MAX_ATTACHMENTS 8

namespace hyperion {

class Framebuffer {
public:
    enum FramebufferAttachment {
        FRAMEBUFFER_ATTACHMENT_NONE = 0b00,
        FRAMEBUFFER_ATTACHMENT_COLOR = 0b01,
        FRAMEBUFFER_ATTACHMENT_NORMALS = 0b10,
        FRAMEBUFFER_ATTACHMENT_POSITIONS = 0b100,
        FRAMEBUFFER_ATTACHMENT_USERDATA = 0b1000,
        FRAMEBUFFER_ATTACHMENT_SSAO = 0b10000,
        FRAMEBUFFER_ATTACHMENT_TANGENTS = 0b100000,
        FRAMEBUFFER_ATTACHMENT_BITANGENTS = 0b1000000,
        FRAMEBUFFER_ATTACHMENT_DEPTH = 0b10000000
    };

    // convert from attachment (2^x) into ordinal (0-5) for use as an array index
    static inline uint64_t AttachmentToOrdinal(FramebufferAttachment attachment)
        { return FastLog2(uint64_t(attachment)); }

    // convert from ordinal (0-5) into power-of-two for use as bit flags
    static inline FramebufferAttachment OrdinalToAttachment(uint64_t ordinal)
    {
        AssertExit(ordinal < FRAMEBUFFER_MAX_ATTACHMENTS);

        return FramebufferAttachment(1 << ordinal);
    }

    struct FramebufferTextureAttributes {
        const char * const material_key;
        int format;
        int internal_format;
        int min_filter;
        int mag_filter;
        bool is_volatile; // can change between post processing passes?

        FramebufferTextureAttributes(const char * const material_key, int format, int internal_format, int min_filter, int mag_filter, bool is_volatile)
            : material_key(material_key),
              format(format),
              internal_format(internal_format),
              min_filter(min_filter),
              mag_filter(mag_filter),
              is_volatile(is_volatile)
        {
        }
    };

    static const std::array<const FramebufferTextureAttributes, FRAMEBUFFER_MAX_ATTACHMENTS> default_texture_attributes;

    using FramebufferAttachments_t = std::array<std::shared_ptr<Texture>, FRAMEBUFFER_MAX_ATTACHMENTS>;

    Framebuffer(int width, int height);
    virtual ~Framebuffer();

    inline unsigned int GetId() const { return id; }
    inline int GetWidth() const { return width; }
    inline int GetHeight() const { return height; }

    inline bool HasAttachment(FramebufferAttachment attachment) const { return m_attachments[AttachmentToOrdinal(attachment)] != nullptr; }
    inline std::shared_ptr<Texture> &GetAttachment(FramebufferAttachment attachment) { return m_attachments[AttachmentToOrdinal(attachment)]; }
    inline const std::shared_ptr<Texture> &GetAttachment(FramebufferAttachment attachment) const { return m_attachments[AttachmentToOrdinal(attachment)]; }

    inline FramebufferAttachments_t &GetAttachments() { return m_attachments; }
    inline const FramebufferAttachments_t &GetAttachments() const { return m_attachments; }

    virtual void Store(FramebufferAttachment attachment, std::shared_ptr<Texture> &texture) = 0;

    virtual void Use() = 0;
    void End();

protected:
    unsigned int id;
    int width, height;
    bool is_created, is_uploaded;

    FramebufferAttachments_t m_attachments;
};

} // namespace hyperion

#endif
