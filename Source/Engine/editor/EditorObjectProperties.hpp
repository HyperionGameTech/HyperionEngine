/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/math/Vector2.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Vector4.hpp>

#include <Core/memory/RefCountedPtr.hpp>
namespace Hyperion {

class Class;

class UIStage;
class UIObject;

class EditorObjectPropertiesBase
{
protected:
    EditorObjectPropertiesBase(TypeId typeId);

public:
    EditorObjectPropertiesBase(const EditorObjectPropertiesBase&) = delete;
    EditorObjectPropertiesBase& operator=(const EditorObjectPropertiesBase&) = delete;
    EditorObjectPropertiesBase(EditorObjectPropertiesBase&&) noexcept = delete;
    EditorObjectPropertiesBase& operator=(EditorObjectPropertiesBase&&) noexcept = delete;
    virtual ~EditorObjectPropertiesBase() = default;

    const Class* GetClass() const;

    virtual Handle<UIObject> CreateUIObject(UIObject* parent) const = 0;

private:
    TypeId m_typeId;
};

template <class T>
class EditorObjectProperties;

template <>
class EditorObjectProperties<Vec2f> : public EditorObjectPropertiesBase
{
public:
    EditorObjectProperties()
        : EditorObjectPropertiesBase(TypeId::ForType<Vec2f>())
    {
    }

    Handle<UIObject> CreateUIObject(UIObject* parent) const override;
};

} // namespace Hyperion
