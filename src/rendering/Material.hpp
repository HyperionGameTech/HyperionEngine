/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MATERIAL_HPP
#define HYPERION_MATERIAL_HPP

#include <rendering/Texture.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <core/utilities/DataMutationState.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>

#include <Types.hpp>

#include <util/EnumOptions.hpp>
#include <HashCode.hpp>

namespace hyperion {

class HYP_API Material : public BasicObject<Material>
{
public:
    static constexpr uint max_parameters = 32u;
    static constexpr uint max_textures = 32u;

    using TextureKeyType = uint64;

    enum TextureKey : TextureKeyType
    {
        MATERIAL_TEXTURE_NONE                           = 0,

        MATERIAL_TEXTURE_ALBEDO_MAP                     = 1 << 0,
        MATERIAL_TEXTURE_NORMAL_MAP                     = 1 << 1,
        MATERIAL_TEXTURE_AO_MAP                         = 1 << 2,
        MATERIAL_TEXTURE_PARALLAX_MAP                   = 1 << 3,
        MATERIAL_TEXTURE_METALNESS_MAP                  = 1 << 4,
        MATERIAL_TEXTURE_ROUGHNESS_MAP                  = 1 << 5,
        MATERIAL_TEXTURE_RADIANCE_MAP                   = 1 << 6,
        MATERIAL_TEXTURE_IRRADIANCE_MAP                 = 1 << 7,
        MATERIAL_TEXTURE_RESERVED0                      = 1 << 8,
        MATERIAL_TEXTURE_RESERVED1                      = 1 << 9,
        MATERIAL_TEXTURE_RESERVED2                      = 1 << 10,
        MATERIAL_TEXTURE_RESERVED3                      = 1 << 11,
        MATERIAL_TEXTURE_RESERVED4                      = 1 << 12,
        MATERIAL_TEXTURE_RESERVED5                      = 1 << 13,

        // terrain

        MATERIAL_TEXTURE_SPLAT_MAP                      = 1 << 14,

        MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP         = 1 << 15,
        MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP        = 1 << 16,
        MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP            = 1 << 17,
        MATERIAL_TEXTURE_BASE_TERRAIN_PARALLAX_MAP      = 1 << 18,

        MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP       = 1 << 19,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP      = 1 << 20,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP          = 1 << 21,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_PARALLAX_MAP    = 1 << 22,

        MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP       = 1 << 23,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP      = 1 << 24,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP          = 1 << 25,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_PARALLAX_MAP    = 1 << 26
    };

    struct Parameter
    {
        union
        {
            float   float_values[4];
            int     int_values[4];
        } values;

        enum Type : uint32
        {
            MATERIAL_PARAMETER_TYPE_NONE,
            MATERIAL_PARAMETER_TYPE_FLOAT,
            MATERIAL_PARAMETER_TYPE_FLOAT2,
            MATERIAL_PARAMETER_TYPE_FLOAT3,
            MATERIAL_PARAMETER_TYPE_FLOAT4,
            MATERIAL_PARAMETER_TYPE_INT,
            MATERIAL_PARAMETER_TYPE_INT2,
            MATERIAL_PARAMETER_TYPE_INT3,
            MATERIAL_PARAMETER_TYPE_INT4
        } type;

        Parameter() : type(MATERIAL_PARAMETER_TYPE_NONE) {}

        template <SizeType Size>
        explicit Parameter(FixedArray<float, Size> &&v)
            : Parameter(v.Data(), Size)
        {
        }
        
        explicit Parameter(const float *v, SizeType count)
            : type(Type(MATERIAL_PARAMETER_TYPE_FLOAT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            std::memcpy(values.float_values, v, count * sizeof(float));
        }
        
        Parameter(float value)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT)
        {
            std::memcpy(values.float_values, &value, sizeof(float));
        }

        Parameter(const Vector2 &xy)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT2)
        {
            std::memcpy(values.float_values, &xy.values, 2 * sizeof(float));
        }

        Parameter(const Vector3 &xyz)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT3)
        {
            std::memcpy(values.float_values, &xyz.values, 3 * sizeof(float));
        }

        Parameter(const Vector4 &xyzw)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT4)
        {
            std::memcpy(values.float_values, &xyzw.values, 4 * sizeof(float));
        }

        Parameter(const Color &color)
            : Parameter(Vector4(color.GetRed(), color.GetGreen(), color.GetBlue(), color.GetAlpha()))
        {
        }

        template <SizeType Size>
        explicit Parameter(FixedArray<int32, Size> &&v)
            : Parameter(v.Data(), Size)
        {
        }
        
        explicit Parameter(const int32 *v, SizeType count)
            : type(Type(MATERIAL_PARAMETER_TYPE_INT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            std::memcpy(values.int_values, v, count * sizeof(int32));
        }

        Parameter(const Parameter &other)
            : type(other.type)
        {
            std::memcpy(&values, &other.values, sizeof(values));
        }

        Parameter &operator=(const Parameter &other)
        {
            type = other.type;
            std::memcpy(&values, &other.values, sizeof(values));

            return *this;
        }

        ~Parameter() = default;

        HYP_FORCE_INLINE bool IsIntType() const
            { return type >= MATERIAL_PARAMETER_TYPE_INT && type <= MATERIAL_PARAMETER_TYPE_INT4; }

        HYP_FORCE_INLINE bool IsFloatType() const
            { return type >= MATERIAL_PARAMETER_TYPE_FLOAT && type <= MATERIAL_PARAMETER_TYPE_FLOAT4; }

        HYP_FORCE_INLINE uint Size() const
        {
            if (type == MATERIAL_PARAMETER_TYPE_NONE) {
                return 0u;
            }

            if (type >= MATERIAL_PARAMETER_TYPE_INT) {
                return uint(type - MATERIAL_PARAMETER_TYPE_INT) + 1;
            }

            return uint(type);
        }

        HYP_FORCE_INLINE void Copy(uint8 *dst) const
            { std::memcpy(dst, &values, Size()); }

        HYP_FORCE_INLINE bool operator==(const Parameter &other) const
            { return std::memcmp(&values, &other.values, sizeof(values)) == 0; }

        HYP_FORCE_INLINE explicit operator int() const
            { return values.int_values[0]; }

        HYP_FORCE_INLINE explicit operator Vec2i() const
            { return Vec2i { values.int_values[0], values.int_values[1] }; }

        HYP_FORCE_INLINE explicit operator Vec3i() const
            { return Vec3i { values.int_values[0], values.int_values[1], values.int_values[2] }; }

        HYP_FORCE_INLINE explicit operator Vec4i() const
            { return Vec4i { values.int_values[0], values.int_values[1], values.int_values[2], values.int_values[3] }; }

        HYP_FORCE_INLINE explicit operator float() const
            { return values.float_values[0]; }

        HYP_FORCE_INLINE explicit operator Vec2f() const
            { return Vec2f { values.float_values[0], values.float_values[1] }; }

        HYP_FORCE_INLINE explicit operator Vec3f() const
            { return Vec3f { values.float_values[0], values.float_values[1], values.float_values[2] }; }

        HYP_FORCE_INLINE explicit operator Vec4f() const
            { return Vec4f { values.float_values[0], values.float_values[1], values.float_values[2], values.float_values[3] }; }

        HYP_FORCE_INLINE HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(int(type));

            if (IsIntType()) {
                hc.Add(values.int_values[0]);
                hc.Add(values.int_values[1]);
                hc.Add(values.int_values[2]);
                hc.Add(values.int_values[3]);
            } else if (IsFloatType()) {
                hc.Add(values.float_values[0]);
                hc.Add(values.float_values[1]);
                hc.Add(values.float_values[2]);
                hc.Add(values.float_values[3]);
            }

            return hc;
        }
    };

    enum State
    {
        MATERIAL_STATE_CLEAN,
        MATERIAL_STATE_DIRTY
    };
    
    enum MaterialKey : uint64
    {
        MATERIAL_KEY_NONE = 0,

        // basic
        MATERIAL_KEY_ALBEDO                 = 1 << 0,
        MATERIAL_KEY_METALNESS              = 1 << 1,
        MATERIAL_KEY_ROUGHNESS              = 1 << 2,
        MATERIAL_KEY_TRANSMISSION           = 1 << 3,
        MATERIAL_KEY_EMISSIVE               = 1 << 4,  // UNUSED
        MATERIAL_KEY_SPECULAR               = 1 << 5,  // UNUSED
        MATERIAL_KEY_SPECULAR_TINT          = 1 << 6,  // UNUSED
        MATERIAL_KEY_ANISOTROPIC            = 1 << 7,  // UNUSED
        MATERIAL_KEY_SHEEN                  = 1 << 8,  // UNUSED
        MATERIAL_KEY_SHEEN_TINT             = 1 << 9,  // UNUSED
        MATERIAL_KEY_CLEARCOAT              = 1 << 10,  // UNUSED
        MATERIAL_KEY_CLEARCOAT_GLOSS        = 1 << 11, // UNUSED
        MATERIAL_KEY_SUBSURFACE             = 1 << 12,  // UNUSED
        MATERIAL_KEY_NORMAL_MAP_INTENSITY   = 1 << 13,
        MATERIAL_KEY_UV_SCALE               = 1 << 14,
        MATERIAL_KEY_PARALLAX_HEIGHT        = 1 << 15,
        MATERIAL_KEY_ALPHA_THRESHOLD        = 1 << 16,
        MATERIAL_KEY_RESERVED2              = 1 << 17,

        // terrain
        MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT = 1 << 18,
        MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT = 1 << 19,
        MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT = 1 << 20,
        MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT = 1 << 21
    };

    using ParameterTable = EnumOptions<MaterialKey, Parameter, max_parameters>;
    using TextureSet = EnumOptions<TextureKey, Handle<Texture>, max_textures>;

    /*! \brief Default parameters for a Material. */
    static ParameterTable DefaultParameters();

    Material();
    Material(
        Name name,
        Bucket bucket = Bucket::BUCKET_OPAQUE
    );
    Material(
        Name name,
        const MaterialAttributes &attributes,
        const ParameterTable &parameters,
        const TextureSet &textures
    );
    Material(const Material &other)                 = delete;
    Material &operator=(const Material &other)      = delete;
    Material(Material &&other) noexcept             = delete;
    Material &operator=(Material &&other) noexcept  = delete;
    ~Material();

    /*! \brief Get the current mutation state of this Material.
        \return The current mutation state of this Material */
    HYP_FORCE_INLINE DataMutationState GetMutationState() const
        { return m_mutation_state; }

    HYP_FORCE_INLINE const ShaderRef &GetShader() const
        { return m_shader; }

    void SetShader(const ShaderRef &shader);

    HYP_FORCE_INLINE ParameterTable &GetParameters()
        { return m_parameters; }

    HYP_FORCE_INLINE const ParameterTable &GetParameters() const
        { return m_parameters; }

    HYP_FORCE_INLINE const Parameter &GetParameter(MaterialKey key) const
        { return m_parameters.Get(key); }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, float>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
        { return m_parameters.Get(key).values.float_values[0]; }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, int>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
        { return m_parameters.Get(key).values.int_values[0]; }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<decltype(T::values[0])>, float>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
    {
        static_assert(sizeof(T::values) <= sizeof(Parameter::values), "T must have a size <= to the size of Parameter::values");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        T result;
        std::memcpy(&result.values[0], &m_parameters.Get(key).values.float_values[0], sizeof(float) * ArraySize(result.values));
        return result;
    }

    /*! \brief Set a parameter on this material with the given key and value
     *  \param key The key of the parameter to be set
     *  \param value The value of the parameter to be set */
    void SetParameter(MaterialKey key, const Parameter &value);

    /*! \brief Set all parameters on this Material to the given ParameterTable.
     *  \param parameters The ParameterTable to set. */
    void SetParameters(const ParameterTable &parameters);

    /*! \brief Set all parameters back to their default values. */
    void ResetParameters();

    /*! \brief Sets the texture with the given key on this Material.
     *  If the Material has already been initialized, the Texture is initialized.
     *  Otherwise, it will be initialized when the Material is initialized.
     *  \param key The texture slot to set the texture on
     *  \param texture The Texture handle to set. */
    void SetTexture(TextureKey key, Handle<Texture> &&texture);

    /*! \brief Sets the texture with the given key on this Material.
     *  If the Material has already been initialized, the Texture is initialized.
     *  Otherwise, it will be initialized when the Material is initialized.
     *  \param key The texture slot to set the texture on
     *  \param texture The Texture handle to set. */
    void SetTexture(TextureKey key, const Handle<Texture> &texture);

    /*! \brief Sets the texture at the given index on this Material.
     *  If the Material has already been initialized, the Texture is initialized.
     *  Otherwise, it will be initialized when the Material is initialized.
     *  \param index The index to set the texture in
     *  \param texture The Texture handle to set. */
    void SetTextureAtIndex(uint index, const Handle<Texture> &texture);

    /*! \brief Sets all textures on this Material to the given TextureSet.
     *  If the Material has already been initialized, the Textures are initialized.
     *  Otherwise, they will be initialized when the Material is initialized.
     *  \param textures The TextureSet to set. */
    void SetTextures(const TextureSet &textures);

    HYP_FORCE_INLINE TextureSet &GetTextures()
        { return m_textures; }

    HYP_FORCE_INLINE const TextureSet &GetTextures() const
        { return m_textures; }

    /*! \brief Return a pointer to a Texture set on this Material by the given
     *  texture key. If no Texture was set, nullptr is returned.
     *  \param key The key of the texture to find
     *  \return Handle for the found Texture, or an empty Handle if not found. */
    const Handle<Texture> &GetTexture(TextureKey key) const;

    /*! \brief Return a pointer to a Texture set on this Material by the given
     *  index. If no Texture was set, nullptr is returned.
     *  \param index The index of the texture to find
     *  \return Handle for the found Texture, or an empty Handle if not found. */
    const Handle<Texture> &GetTextureAtIndex(uint index) const;

    /*! \brief Get the bucket for this Material.
     *  \return The bucket for this Material. */
    HYP_FORCE_INLINE Bucket GetBucket() const
        { return m_render_attributes.bucket; }

    /*! \brief Set the bucket for this Material.
     *  \param bucket The bucket to set. */
    HYP_FORCE_INLINE void SetBucket(Bucket bucket)
        { m_render_attributes.bucket = bucket; }

    /*! \brief Get whether this Material is alpha blended.
     *  \return True if the Material is alpha blended, false otherwise. */
    HYP_FORCE_INLINE bool IsAlphaBlended() const
        { return m_render_attributes.blend_function != BlendFunction::None(); }

    /*! \brief Set whether this Material is alpha blended.
     *  \param is_alpha_blended True if the Material is alpha blended, false otherwise.
     *  \param blend_function The blend function to use if the Material is alpha blended. By default, it is set to \ref{BlendFunction::AlphaBlending()}. */
    HYP_FORCE_INLINE void SetIsAlphaBlended(bool is_alpha_blended, BlendFunction blend_function = BlendFunction::AlphaBlending())
    {
        if (is_alpha_blended) {
            m_render_attributes.blend_function = blend_function;
        } else {
            m_render_attributes.blend_function = BlendFunction::None();
        }
    }

    /*! \brief Get the blend function for this Material.
     *  \return The blend function for this Material. */
    HYP_FORCE_INLINE BlendFunction GetBlendFunction() const
        { return m_render_attributes.blend_function; }

    /*! \brief Set the blend function for this Material.
     *  \param blend_function The blend function to set. */
    HYP_FORCE_INLINE void SetBlendMode(BlendFunction blend_function)
        { m_render_attributes.blend_function = blend_function; }

    /*! \brief Get whether depth writing is enabled for this Material.
     *  \return True if depth writing is enabled, false otherwise. */
    HYP_FORCE_INLINE bool IsDepthWriteEnabled() const
        { return bool(m_render_attributes.flags & MaterialAttributeFlags::DEPTH_WRITE); }

    /*! \brief Set whether depth writing is enabled for this Material.
     *  \param is_depth_write_enabled True if depth writing is enabled, false otherwise. */
    HYP_FORCE_INLINE void SetIsDepthWriteEnabled(bool is_depth_write_enabled)
    {
        if (is_depth_write_enabled) {
            m_render_attributes.flags |= MaterialAttributeFlags::DEPTH_WRITE;
        } else {
            m_render_attributes.flags &= ~MaterialAttributeFlags::DEPTH_WRITE;
        }
    }

    /*! \brief Get whether depth testing is enabled for this Material.
     *  \return True if depth testing is enabled, false otherwise. */
    HYP_FORCE_INLINE bool IsDepthTestEnabled() const
        { return bool(m_render_attributes.flags & MaterialAttributeFlags::DEPTH_TEST); }

    /*! \brief Set whether depth testing is enabled for this Material.
     *  \param is_depth_test_enabled True if depth testing is enabled, false otherwise. */
    HYP_FORCE_INLINE void SetIsDepthTestEnabled(bool is_depth_test_enabled)
    {
        if (is_depth_test_enabled) {
            m_render_attributes.flags |= MaterialAttributeFlags::DEPTH_TEST;
        } else {
            m_render_attributes.flags &= ~MaterialAttributeFlags::DEPTH_TEST;
        }
    }

    /*! \brief Get the face culling mode for this Material.
     *  \return The face culling mode for this Material. */
    HYP_FORCE_INLINE FaceCullMode GetFaceCullMode() const
        { return m_render_attributes.cull_faces; }

    /*! \brief Set the face culling mode for this Material.
     *  \param cull_mode The face culling mode to set. */
    HYP_FORCE_INLINE void SetFaceCullMode(FaceCullMode cull_mode)
        { m_render_attributes.cull_faces = cull_mode; }

    /*! \brief Get the render attributes of this Material.
     *  \return The render attributes of this Material. */
    HYP_FORCE_INLINE MaterialAttributes &GetRenderAttributes()
        { return m_render_attributes; }

    /*! \brief Get the render attributes of this Material.
     *  \return The render attributes of this Material. */
    HYP_FORCE_INLINE const MaterialAttributes &GetRenderAttributes() const
        { return m_render_attributes; }

    /*! \brief If a Material is static, it is expected to not change frequently and
     *  may be shared across many objects. Otherwise, it is considered dynamic and may
     *  be modified.
     *  \return True if the Material is static, false if it is dynamic. */
    HYP_FORCE_INLINE bool IsStatic() const
        { return !m_is_dynamic; }

    /*! \brief If a Material is
     *  dynamic, it is expected to change frequently and may be modified. Otherwise,
     *  it is considered static and should not be modified as it may be shared across many
     *  objects.
     *  \return True if the Material is dynamic, false if it is static. */
    HYP_FORCE_INLINE bool IsDynamic() const
        { return m_is_dynamic; }

    HYP_FORCE_INLINE void SetIsDynamic(bool is_dynamic)
        { m_is_dynamic = is_dynamic; }

    void Init();

    /*! \brief If the Material's mutation state is dirty, this will
     * create a task on the render thread to update the Material's
     * data on the GPU. */
    void EnqueueRenderUpdates();

    /*! \brief Clone this Material. The cloned Material will have the same
     *  shader, parameters, textures, and render attributes as the original.
     *  \details Using this method is a good way to get around the fact that
     *  static Materials are shared across many objects. If you need to modify
     *  a static Material, clone it first. The cloned Material will be dynamic
     *  by default, and can be modified without affecting the original Material.
     *  \note The cloned Material will not be initialized.
     *  \return A new Material that is a clone of this Material.
     */
    Handle<Material> Clone() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_shader.GetHashCode());
        hc.Add(m_parameters.GetHashCode());
        hc.Add(m_textures.GetHashCode());
        hc.Add(m_render_attributes.GetHashCode());

        return hc;
    }

private:
    void EnqueueTextureUpdate(TextureKey key);
    void EnqueueDescriptorSetCreate();
    void EnqueueDescriptorSetDestroy();

    ShaderRef                                           m_shader;

    ParameterTable                                      m_parameters;
    TextureSet                                          m_textures;

    MaterialAttributes                                  m_render_attributes;

    bool                                                m_is_dynamic;

    MaterialShaderData                                  m_shader_data;
    mutable DataMutationState                           m_mutation_state;
};

class MaterialGroup : public BasicObject<MaterialGroup>
{
public:
    MaterialGroup();
    MaterialGroup(const MaterialGroup &other) = delete;
    MaterialGroup &operator=(const MaterialGroup &other) = delete;
    ~MaterialGroup();

    void Init();
    void Add(const String &name, Handle<Material> &&material);
    bool Remove(const String &name);

    Handle<Material> &Get(const String &name)
        { return m_materials[name]; }

    const Handle<Material> &Get(const String &name) const
        { return m_materials.At(name); }

    bool Has(const String &name) const
        { return m_materials.Contains(name); }

private:
    HashMap<String, Handle<Material>> m_materials;
};

class HYP_API MaterialCache
{
public:
    static MaterialCache *GetInstance();

    void Add(const Handle<Material> &material);

    Handle<Material> CreateMaterial(
        MaterialAttributes attributes = { },
        const Material::ParameterTable &parameters = Material::DefaultParameters(),
        const Material::TextureSet &textures = { }
    );

    Handle<Material> GetOrCreate(
        MaterialAttributes attributes = { },
        const Material::ParameterTable &parameters = Material::DefaultParameters(),
        const Material::TextureSet &textures = { }
    );

private:
    FlatMap<HashCode::ValueType, WeakHandle<Material>>  m_map;
    Mutex                                               m_mutex;
};

class HYP_API MaterialDescriptorSetManager
{
public:
    MaterialDescriptorSetManager();
    MaterialDescriptorSetManager(const MaterialDescriptorSetManager &other)                 = delete;
    MaterialDescriptorSetManager &operator=(const MaterialDescriptorSetManager &other)      = delete;
    MaterialDescriptorSetManager(MaterialDescriptorSetManager &&other) noexcept             = delete;
    MaterialDescriptorSetManager &operator=(MaterialDescriptorSetManager &&other) noexcept  = delete;
    ~MaterialDescriptorSetManager();

    /*! \brief Get the descriptor set for the given material and frame index. Only
     *   to be used from the render thread. If the descriptor set is not found, nullptr
     *   is returned.
     *   \param material The IDof material to get the descriptor set for
     *   \param frame_index The frame index to get the descriptor set for
     *   \returns The descriptor set for the given material and frame index or nullptr if not found
     */
    const DescriptorSetRef &GetDescriptorSet(ID<Material> material, uint frame_index) const;

    /*! \brief Add a material to the manager. This will create a descriptor set for
     *  the material and add it to the manager. Usable from any thread.
     *  \note If this function is called form the render thread, the descriptor set will
     *  be created immediately. If called from any other thread, the descriptor set will
     *  be created on the next call to Update.
     *  \param id The ID of the material to add
     */
    void AddMaterial(ID<Material> id);

    /*! \brief Add a material to the manager. This will create a descriptor set for
     *  the material and add it to the manager. Usable from any thread.
     *  \note If this function is called form the render thread, the descriptor set will
     *  be created immediately. If called from any other thread, the descriptor set will
     *  be created on the next call to Update.
     *  \param id The ID of the material to add
     *  \param textures The textures to add to the material
     */
    void AddMaterial(ID<Material> id, FixedArray<Handle<Texture>, max_bound_textures> &&textures);

    /*! \brief Remove a material from the manager. This will remove the descriptor set
     *  for the material from the manager. Usable from any thread.
     *  \param material The ID of the material to remove
     */
    void EnqueueRemoveMaterial(ID<Material> material);

    /*! \brief Remove a material from the manager. Only to be used from the render thread.
     *  \param material The ID of the material to remove
     */
    void RemoveMaterial(ID<Material> material);

    void SetNeedsDescriptorSetUpdate(ID<Material> id);

    /*! \brief Initialize the MaterialDescriptorSetManager - Only to be used by the owning Engine instance. */
    void Initialize();

    /*! \brief Process any pending additions or removals. Usable from the render thread. */
    void UpdatePendingDescriptorSets(Frame *frame);

    /*! \brief Update the descriptor sets for the given frame. Usable from the render thread. */
    void Update(Frame *frame);

private:
    void CreateInvalidMaterialDescriptorSet();

    HashMap<ID<Material>, FixedArray<DescriptorSetRef, max_frames_in_flight>>       m_material_descriptor_sets;

    Array<Pair<ID<Material>, FixedArray<DescriptorSetRef, max_frames_in_flight>>>   m_pending_addition;
    Array<ID<Material>>                                                             m_pending_removal;
    Mutex                                                                           m_pending_mutex;
    AtomicVar<bool>                                                                 m_pending_addition_flag;

    FixedArray<Array<ID<Material>>, max_frames_in_flight>                           m_descriptor_sets_to_update;
    Mutex                                                                           m_descriptor_sets_to_update_mutex;
    AtomicVar<uint>                                                                 m_descriptor_sets_to_update_flag;
};

} // namespace hyperion

#endif
