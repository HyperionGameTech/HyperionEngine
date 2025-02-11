/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHT_HPP
#define HYPERION_LIGHT_HPP

#include <core/Base.hpp>

#include <core/containers/Bitset.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/Material.hpp>
#include <rendering/RenderResources.hpp>

#include <math/Vector3.hpp>
#include <math/BoundingBox.hpp>
#include <math/BoundingSphere.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Camera;

enum class LightType : uint32
{
    DIRECTIONAL,
    POINT,
    SPOT,
    AREA_RECT,

    MAX
};

struct LightShaderData
{
    uint32  light_id;
    uint32  light_type;
    uint32  color_packed;
    float   radius;
    // 16

    float   falloff;
    uint32  shadow_map_index;
    Vec2f   area_size;
    // 32

    Vec4f   position_intensity;
    Vec4f   normal;
    // 64

    Vec2f   spot_angles;
    uint32  material_index;
    uint32  _pad2;

    Vec4u   pad3;
    Vec4u   pad4;
    Vec4u   pad5;
};

static_assert(sizeof(LightShaderData) == 128);

static constexpr uint32 max_lights = (64ull * 1024ull) / sizeof(LightShaderData);

class LightRenderResources final : public RenderResourcesBase
{
public:
    LightRenderResources(Light *light);
    LightRenderResources(LightRenderResources &&other) noexcept;
    virtual ~LightRenderResources() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE Light *GetLight() const
        { return m_light; }

    void SetMaterial(const Handle<Material> &material);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const Handle<Material> &GetMaterial() const
        { return m_material; }

    void SetBufferData(const LightShaderData &buffer_data);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const LightShaderData &GetBufferData() const
        { return m_buffer_data; }

    void SetVisibilityBits(const Bitset &visibility_bits);
    
    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const Bitset &GetVisibilityBits() const
        { return m_visibility_bits; }

protected:
    virtual void Initialize() override;
    virtual void Destroy() override;
    virtual void Update() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

    virtual Name GetTypeName() const override
        { return NAME("LightRenderResources"); }

private:
    void UpdateBufferData();

    Light                                       *m_light;
    Bitset                                      m_visibility_bits;
    Handle<Material>                            m_material;
    TResourceHandle<MaterialRenderResources>    m_material_render_resources_handle;
    LightShaderData                             m_buffer_data;
};

HYP_CLASS()
class HYP_API Light : public HypObject<Light>
{
    HYP_OBJECT_BODY(Light);

public:
    Light();

    Light(
        LightType type,
        const Vec3f &position,
        const Color &color,
        float intensity,
        float radius
    );

    Light(
        LightType type,
        const Vec3f &position,
        const Vec3f &normal,
        const Vec2f &area_size,
        const Color &color,
        float intensity,
        float radius
    );

    Light(const Light &other)                   = delete;
    Light &operator=(const Light &other)        = delete;

    Light(Light &&other) noexcept               = delete;
    Light &operator=(Light &&other) noexcept    = delete;

    ~Light();

    HYP_FORCE_INLINE LightRenderResources &GetRenderResources() const
        { return *m_render_resources; }

    /*! \brief Get the current mutation state of the light.
     *
     *  \return The mutation state.
     */
    HYP_FORCE_INLINE DataMutationState GetMutationState() const
        { return m_mutation_state; }

    /*! \brief Get the type of the light.
     *
     *  \return The type.
     */
    HYP_METHOD(Property="Type", Serialize=true)
    LightType GetLightType() const
        { return m_type; }

    /*! \brief Set the type of the light. */
    HYP_METHOD(Property="Type", Serialize=true)
    void SetLightType(LightType type)
    {
        if (m_type == type) {
            return;
        }

        m_type = type;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the position for the light. For directional lights, this is the direction the light is pointing.
     *
     *  \return The position or direction. */
    HYP_METHOD(Property="Position", Serialize=true, Editor=true)
    const Vec3f &GetPosition() const
        { return m_position; }

    /*! \brief Set the position for the light. For directional lights, this is the direction the light is pointing.
     *
     *  \param position The position or direction to set. */
    HYP_METHOD(Property="Position", Serialize=true, Editor=true)
    void SetPosition(const Vec3f &position)
    {
        if (m_position == position) {
            return;
        }

        m_position = position;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the normal for the light. This is used only for area lights.
     *
     *  \return The normal. */
    HYP_METHOD(Property="Normal", Serialize=true, Editor=true)
    const Vec3f &GetNormal() const
        { return m_normal; }

    /*! \brief Set the normal for the light. This is used only for area lights.
     *
     *  \param normal The normal to set. */
    HYP_METHOD(Property="Normal", Serialize=true, Editor=true)
    void SetNormal(const Vec3f &normal)
    {
        if (m_normal == normal) {
            return;
        }

        m_normal = normal;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the area size for the light. This is used only for area lights.
     *
     *  \return The area size. (x = width, y = height) */
    HYP_METHOD(Property="AreaSize", Serialize=true, Editor=true)
    const Vec2f &GetAreaSize() const
        { return m_area_size; }

    /*! \brief Set the area size for the light. This is used only for area lights.
     *
     *  \param area_size The area size to set. (x = width, y = height) */
    HYP_METHOD(Property="AreaSize", Serialize=true, Editor=true)
    void SetAreaSize(const Vec2f &area_size)
    {
        if (m_area_size == area_size) {
            return;
        }

        m_area_size = area_size;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the color for the light.
     *
     *  \return The color. */
    HYP_METHOD(Property="Color", Serialize=true, Editor=true)
    const Color &GetColor() const
        { return m_color; }

    /*! \brief Set the color for the light.
     *
     *  \param color The color to set. */
    HYP_METHOD(Property="Color", Serialize=true, Editor=true)
    void SetColor(const Color &color)
    {
        if (m_color == color) {
            return;
        }

        m_color = color;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the intensity for the light. This is used to determine how bright the light is.
     *
     *  \return The intensity. */
    HYP_METHOD(Property="Intensity", Serialize=true, Editor=true)
    float GetIntensity() const
        { return m_intensity; }

    /*! \brief Set the intensity for the light. This is used to determine how bright the light is.
     *
     *  \param intensity The intensity to set. */
    HYP_METHOD(Property="Intensity", Serialize=true, Editor=true)
    void SetIntensity(float intensity)
    {
        if (m_intensity == intensity) {
            return;
        }

        m_intensity = intensity;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \return The radius. */
    HYP_METHOD(Property="Radius", Serialize=true, Editor=true)
    float GetRadius() const
    {
        switch (m_type) {
        case LightType::DIRECTIONAL:
            return INFINITY;
        case LightType::POINT:
            return m_radius;
        default:
            return 0.0f;
        }
    }

    /*! \brief Set the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \param radius The radius to set. */
    HYP_METHOD(Property="Radius", Serialize=true, Editor=true)
    void SetRadius(float radius)
    {
        if (m_radius == radius) {
            return;
        }

        m_radius = radius;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     *  \return The falloff. */
    HYP_METHOD(Property="Falloff", Serialize=true, Editor=true)
    float GetFalloff() const
        { return m_falloff; }

    /*! \brief Set the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     *  \param falloff The falloff to set. */
    HYP_METHOD(Property="Falloff", Serialize=true, Editor=true)
    void SetFalloff(float falloff)
    {
        if (m_falloff == falloff) {
            return;
        }

        m_falloff = falloff;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \return The spotlight angles. */
    HYP_METHOD(Property="SpotAngles", Serialize=true, Editor=true)
    const Vec2f &GetSpotAngles() const
        { return m_spot_angles; }

    /*! \brief Set the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \param spot_angles The angles to set for the spotlight. */
    HYP_METHOD(Property="SpotAngles", Serialize=true, Editor=true)
    void SetSpotAngles(const Vec2f &spot_angles)
    {
        if (m_spot_angles == spot_angles) {
            return;
        }

        m_spot_angles = spot_angles;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the shadow map index for the light. This is used when sampling shadow maps for the particular light.
     *
     *  \return The shadow map index. */
    HYP_FORCE_INLINE uint32 GetShadowMapIndex() const
        { return m_shadow_map_index; }

    /*! \brief Set the shadow map index for the light. This is used when sampling shadow maps for the particular light.
     *
     * \param shadow_map_index The shadow map index to set.
     */
    HYP_FORCE_INLINE void SetShadowMapIndex(uint32 shadow_map_index)
    {
        if (shadow_map_index == m_shadow_map_index) {
            return;
        }

        m_shadow_map_index = shadow_map_index;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the material  for the light. Used for area lights.
     *
     *  \return The material handle associated with the Light. */
    HYP_METHOD(Property="Material", Serialize=true, Editor=true)
    const Handle<Material> &GetMaterial() const
        { return m_material; }

    /*! \brief Sets the material handle associated with the Light. Used for textured area lights.
     *
     *  \param material The material to set for this Light. */
    HYP_METHOD(Property="Material", Serialize=true, Editor=true)
    void SetMaterial(Handle<Material> material);

    /*! \brief Check if the light is set as visible to the camera.
     *
     * \param camera_id The camera to check visibility for.
     * \return True if the light is visible, false otherwise.
     */
    bool IsVisible(ID<Camera> camera_id) const;

    /*! \brief Set the visibility of the light to the camera.
     *
     * \param camera_id The camera to set visibility for.
     * \param is_visible True if the light is visible, false otherwise.
     */
    void SetIsVisible(ID<Camera> camera_id, bool is_visible);

    HYP_METHOD()
    BoundingBox GetAABB() const;

    BoundingSphere GetBoundingSphere() const;

    void Init();
    //void EnqueueBind() const;
    void EnqueueUnbind() const;

    void EnqueueRenderUpdates();

protected:
    LightType                       m_type;
    Vec3f                           m_position;
    Vec3f                           m_normal;
    Vec2f                           m_area_size;
    Color                           m_color;
    float                           m_intensity;
    float                           m_radius;
    float                           m_falloff;
    Vec2f                           m_spot_angles;
    uint32                          m_shadow_map_index;
    Handle<Material>                m_material;

private:
    Pair<Vec3f, Vec3f> CalculateAreaLightRect() const;

    mutable DataMutationState       m_mutation_state;

    Bitset                          m_visibility_bits;

    LightRenderResources            *m_render_resources;
};

class HYP_API DirectionalLight : public Light
{
public:
    static constexpr float default_intensity = 10.0f;

    DirectionalLight(
        Vec3f direction,
        Color color,
        float intensity = default_intensity
    ) : Light(
            LightType::DIRECTIONAL,
            direction,
            color,
            intensity,
            0.0f
        )
    {
    }

    HYP_FORCE_INLINE const Vec3f &GetDirection() const
        { return GetPosition(); }

    HYP_FORCE_INLINE void SetDirection(Vec3f direction)
        { SetPosition(direction); }
};

class HYP_API PointLight : public Light
{
public:
    static constexpr float default_intensity = 5.0f;
    static constexpr float default_radius = 15.0f;

    PointLight(
        Vec3f position,
        Color color,
        float intensity = default_intensity,
        float radius = default_radius
    ) : Light(
            LightType::POINT,
            position,
            color,
            intensity,
            radius
        )
    {
    }
};

class HYP_API SpotLight : public Light
{
public:
    static constexpr float default_intensity = 5.0f;
    static constexpr float default_radius = 15.0f;
    static constexpr float default_outer_angle = 45.0f;
    static constexpr float default_inner_angle = 30.0f;

    SpotLight(
        Vec3f position,
        Vec3f direction,
        Color color,
        float intensity = default_intensity,
        float radius = default_radius,
        Vec2f angles = { default_outer_angle, default_inner_angle }
    ) : Light(
            LightType::SPOT,
            position,
            direction,
            Vec2f(0.0f, 0.0f),
            color,
            intensity,
            radius
        )
    {
        SetSpotAngles(angles);
    }

    HYP_FORCE_INLINE const Vec3f &GetDirection() const
        { return GetNormal(); }

    HYP_FORCE_INLINE void SetDirection(const Vec3f &direction)
        { SetNormal(direction); }
};

class HYP_API RectangleLight : public Light
{
public:
    RectangleLight(
        Vec3f position,
        Vec3f normal,
        Vec2f area_size,
        Color color,
        float intensity = 1.0f,
        float distance = 1.0f
    ) : Light(
            LightType::AREA_RECT,
            position,
            normal,
            area_size,
            color,
            intensity,
            distance
        )
    {
    }
};

} // namespace hyperion

#endif
