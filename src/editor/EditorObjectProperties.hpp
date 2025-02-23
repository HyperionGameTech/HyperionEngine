/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_OBJECT_PROPERTIES_HPP
#define HYPERION_EDITOR_OBJECT_PROPERTIES_HPP

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

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

    virtual RC<UIObject> CreateUIObject(UIObject *parent) const = 0;

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

    RC<UIObject> CreateUIObject(UIObject *parent) const override;
};

} // namespace hyperion

#endif
