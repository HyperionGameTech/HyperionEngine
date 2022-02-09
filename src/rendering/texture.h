#ifndef TEXTURE_H
#define TEXTURE_H

#include "../asset/loadable.h"

namespace hyperion {

class Texture : public Loadable {
    friend class TextureLoader;
public:
    enum TextureType {
        TEXTURE_TYPE_2D = 0,
        TEXTURE_TYPE_3D = 1,
        TEXTURE_TYPE_CUBEMAP = 2
    };

    Texture(TextureType texture_type);
    Texture(TextureType texture_type, int width, int height, unsigned char *bytes);
    Texture(const Texture &other) = delete;
    Texture &operator=(const Texture &other) = delete;
    virtual ~Texture();

    unsigned int GetId() const;

    inline bool IsCreated() const { return is_created; }
    inline bool IsUploaded() const { return is_uploaded; }

    void SetFormat(int type);
    inline int GetFormat() const { return fmt; }
    void SetInternalFormat(int type);
    inline int GetInternalFormat() const { return ifmt; }

    inline int GetWidth() const { return width; }
    inline int GetHeight() const { return height; }

    void SetFilter(int mag, int min);
    inline int GetMagFilter() const { return mag_filter; }
    inline int GetMinFilter() const { return min_filter; }

    void SetWrapMode(int s, int t);
    inline int GetWrapS() const { return wrap_s; }
    inline int GetWrapT() const { return wrap_t; }

    virtual void CopyData(Texture * const other) = 0;

    inline TextureType GetTextureType() const { return m_texture_type; }

    unsigned char * const GetBytes() const { return bytes; }

    static size_t NumComponents(int format);
    static void ActiveTexture(int i);

    void Prepare(bool should_upload_data = true);

    virtual void End() = 0;

    inline void Begin(bool should_upload_data = true)
    {
        Prepare(should_upload_data); 
        Use();
    }

protected:
    unsigned int id;

    int ifmt, fmt, width, height;
    unsigned char *bytes;

    int mag_filter, min_filter;
    int wrap_s, wrap_t;

    bool m_is_storage;

    TextureType m_texture_type;

    virtual void Initialize();
    virtual void Deinitialize();
    virtual void UploadGpuData(bool should_upload_data) = 0;
    virtual void Use() = 0;

private:
    bool is_created, is_uploaded;
};

} // namespace hyperion

#endif
