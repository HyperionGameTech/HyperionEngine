/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_OBJECT_PROPERTIES_HPP
#define HYPERION_EDITOR_OBJECT_PROPERTIES_HPP

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/TypeID.hpp>

namespace hyperion {

class HypClass;

class UIStage;
class UIObject;

class EditorObjectPropertiesBase
{
protected:
    EditorObjectPropertiesBase(TypeID type_id);

public:
    EditorObjectPropertiesBase(const EditorObjectPropertiesBase &)                  = delete;
    EditorObjectPropertiesBase &operator=(const EditorObjectPropertiesBase &)       = delete;
    EditorObjectPropertiesBase(EditorObjectPropertiesBase &&) noexcept              = delete;
    EditorObjectPropertiesBase &operator=(EditorObjectPropertiesBase &&) noexcept   = delete;
    virtual ~EditorObjectPropertiesBase()                                           = default;

    const HypClass *GetClass() const;

    virtual RC<UIObject> CreateUIObject(UIStage *stage) const = 0;

private:
    TypeID  m_type_id;
};

template <class T>
class EditorObjectProperties;

template <>
class EditorObjectProperties<Vec2f> : public EditorObjectPropertiesBase
{
public:
    EditorObjectProperties()
        : EditorObjectPropertiesBase(TypeID::ForType<Vec2f>())
    {
    }

    RC<UIObject> CreateUIObject(UIStage *stage) const override;
};

// template <>
// class EditorObjectProperties<Vec2i> : public EditorObjectPropertiesBase
// {
// public:
//     EditorObjectProperties()
//         : EditorObjectPropertiesBase(TypeID::ForType<Vec2i>())
//     {
//     }

//     RC<UIObject> CreateUIObject(UIStage *stage) const override;
// };

// template <>
// class EditorObjectProperties<Vec2u> : public EditorObjectPropertiesBase
// {
// public:
//     EditorObjectProperties()
//         : EditorObjectPropertiesBase(TypeID::ForType<Vec2u>())
//     {
//     }

//     RC<UIObject> CreateUIObject(UIStage *stage) const override;
// };

// template <>
// class EditorObjectProperties<Vec3f> : public EditorObjectPropertiesBase
// {
// public:
//     EditorObjectProperties()
//         : EditorObjectPropertiesBase(TypeID::ForType<Vec3f>())
//     {
//     }

//     RC<UIObject> CreateUIObject(UIStage *stage) const override;
// };

// template <>
// class EditorObjectProperties<Vec3i> : public EditorObjectPropertiesBase
// {
// public:
//     EditorObjectProperties()
//         : EditorObjectPropertiesBase(TypeID::ForType<Vec3i>())
//     {
//     }

//     RC<UIObject> CreateUIObject(UIStage *stage) const override;
// };

// template <>
// class EditorObjectProperties<Vec3u> : public EditorObjectPropertiesBase
// {
// public:
//     EditorObjectProperties()
//         : EditorObjectPropertiesBase(TypeID::ForType<Vec3u>())
//     {
//     }

//     RC<UIObject> CreateUIObject(UIStage *stage) const override;
// };

// template <>
// class EditorObjectProperties<Vec4f> : public EditorObjectPropertiesBase
// {
// public:
//     EditorObjectProperties()
//         : EditorObjectPropertiesBase(TypeID::ForType<Vec4f>())
//     {
//     }

//     RC<UIObject> CreateUIObject(UIStage *stage) const override;
// };

// template <>
// class EditorObjectProperties<Vec4i> : public EditorObjectPropertiesBase
// {
// public:
//     EditorObjectProperties()
//         : EditorObjectPropertiesBase(TypeID::ForType<Vec4i>())
//     {
//     }

//     RC<UIObject> CreateUIObject(UIStage *stage) const override;
// };

// template <>
// class EditorObjectProperties<Vec4u> : public EditorObjectPropertiesBase
// {
// public:
//     EditorObjectProperties()
//         : EditorObjectPropertiesBase(TypeID::ForType<Vec4u>())
//     {
//     }

//     RC<UIObject> CreateUIObject(UIStage *stage) const override;
// };

} // namespace hyperion

#endif
