#ifndef HYPERION_V2_MATERIAL_H
#define HYPERION_V2_MATERIAL_H

#include <rendering/Texture.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/Shader.hpp>
#include <rendering/ShaderDataState.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <core/lib/FixedArray.hpp>
#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/Mutex.hpp>
#include <core/lib/AtomicVar.hpp>
#include <Types.hpp>

#include <util/EnumOptions.hpp>
#include <HashCode.hpp>

#include <array>
#include <mutex>

namespace hyperion {
namespace renderer {

class DescriptorSet;

} // namespace renderer
} // namespace hyperion

namespace hyperion::v2 {

class HYP_API Material
    : public BasicObject<STUB_CLASS(Material)>
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
            float float_values[4];
            int int_values[4];
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

        bool IsIntType() const
            { return type >= MATERIAL_PARAMETER_TYPE_INT && type <= MATERIAL_PARAMETER_TYPE_INT4; }

        bool IsFloatType() const
            { return type >= MATERIAL_PARAMETER_TYPE_FLOAT && type <= MATERIAL_PARAMETER_TYPE_FLOAT4; }

        uint Size() const
        {
            if (type == MATERIAL_PARAMETER_TYPE_NONE) {
                return 0u;
            }

            if (type >= MATERIAL_PARAMETER_TYPE_INT) {
                return uint(type - MATERIAL_PARAMETER_TYPE_INT) + 1;
            }

            return uint(type);
        }

        void Copy(uint8 *dst) const
        {
            std::memcpy(dst, &values, Size());
        }

        bool operator==(const Parameter &other) const
        {
            return std::memcmp(&values, &other.values, sizeof(values)) == 0;
        }

        explicit operator int() const
            { return values.int_values[0]; }

        explicit operator Vec2i() const
            { return Vec2i { values.int_values[0], values.int_values[1] }; }

        explicit operator Vec3i() const
            { return Vec3i { values.int_values[0], values.int_values[1], values.int_values[2] }; }

        explicit operator Vec4i() const
            { return Vec4i { values.int_values[0], values.int_values[1], values.int_values[2], values.int_values[3] }; }

        explicit operator float() const
            { return values.float_values[0]; }

        explicit operator Vec2f() const
            { return Vec2f { values.float_values[0], values.float_values[1] }; }

        explicit operator Vec3f() const
            { return Vec3f { values.float_values[0], values.float_values[1], values.float_values[2] }; }

        explicit operator Vec4f() const
            { return Vec4f { values.float_values[0], values.float_values[1], values.float_values[2], values.float_values[3] }; }

        HashCode GetHashCode() const
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
    Material(const Material &other) = delete;
    Material &operator=(const Material &other) = delete;
    ~Material();

    ShaderDataState GetShaderDataState() const
        { return m_shader_data_state; }

    void SetShaderDataState(ShaderDataState state)
        { m_shader_data_state = state; }

    const Handle<Shader> &GetShader() const
        { return m_shader; }

    void SetShader(Handle<Shader> shader);

    ParameterTable &GetParameters()
        { return m_parameters; }

    const ParameterTable &GetParameters() const
        { return m_parameters; }

    const Parameter &GetParameter(MaterialKey key) const
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
        static_assert(sizeof(T::values) <= sizeof(Parameter::values));

        T result;
        std::memcpy(&result.values[0], &m_parameters.Get(key).values.float_values[0], sizeof(float) * std::size(result.values));
        return result;
    }

    /*! \brief Set a parameter on this material with the given key and value
     * @param key The key of the parameter to be set
     * @param value The value of the parameter to be set
     */
    void SetParameter(MaterialKey key, const Parameter &value);

    /*! \brief Set all parameters back to their default values. */
    void ResetParameters();

    /*! \brief Sets the texture with the given key on this Material.
     * If the Material has already been initialized, the Texture is initialized.
     * Otherwise, it will be initialized when the Material is initialized.
     * @param key The texture slot to set the texture on
     * @param texture A Texture resource
     */
    void SetTexture(TextureKey key, Handle<Texture> &&texture);

    /*! \brief Sets the texture with the given key on this Material.
     * If the Material has already been initialized, the Texture is initialized.
     * Otherwise, it will be initialized when the Material is initialized.
     * @param key The texture slot to set the texture on
     * @param texture A Texture resource
     */
    void SetTexture(TextureKey key, const Handle<Texture> &texture);

    /*! \brief Sets the texture at the given index on this Material.
     * If the Material has already been initialized, the Texture is initialized.
     * Otherwise, it will be initialized when the Material is initialized.
     * @param index The index to set the texture in
     * @param texture A Texture resource
     */
    void SetTextureAtIndex(uint index, const Handle<Texture> &texture);

    TextureSet &GetTextures()
        { return m_textures; }

    const TextureSet &GetTextures() const
        { return m_textures; }

    /*! \brief Return a pointer to a Texture set on this Material by the given
     * texture key. If no Texture was set, nullptr is returned.
     * @param key The key of the texture to find
     * @returns Pointer to the found Texture, or nullptr.
     */
    const Handle<Texture> &GetTexture(TextureKey key) const;

    /*! \brief Return a pointer to a Texture set on this Material by the given
     * index. If no Texture was set, nullptr is returned.
     * @param index The index of the texture to find
     * @returns Pointer to the found Texture, or nullptr.
     */
    const Handle<Texture> &GetTextureAtIndex(uint index) const;

    Bucket GetBucket() const
        { return m_render_attributes.bucket; }

    void SetBucket(Bucket bucket)
        { m_render_attributes.bucket = bucket; }

    bool IsAlphaBlended() const
        { return m_render_attributes.blend_function != BlendFunction::None(); }

    void SetIsAlphaBlended(bool is_alpha_blended, BlendFunction blend_function = BlendFunction::AlphaBlending())
    {
        if (is_alpha_blended) {
            m_render_attributes.blend_function = blend_function;
        } else {
            m_render_attributes.blend_function = BlendFunction::None();
        }
    }

    BlendFunction GetBlendFunction() const
        { return m_render_attributes.blend_function; }

    void SetBlendMode(BlendFunction blend_function)
        { m_render_attributes.blend_function = blend_function; }

    bool IsDepthWriteEnabled() const
        { return bool(m_render_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE); }

    void SetIsDepthWriteEnabled(bool is_depth_write_enabled)
    {
        if (is_depth_write_enabled) {
            m_render_attributes.flags |= MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE;
        } else {
            m_render_attributes.flags &= ~MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE;
        }
    }

    bool IsDepthTestEnabled() const
        { return bool(m_render_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST); }

    void SetIsDepthTestEnabled(bool is_depth_test_enabled)
    {
        if (is_depth_test_enabled) {
            m_render_attributes.flags |= MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;
        } else {
            m_render_attributes.flags &= ~MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;
        }
    }

    FaceCullMode GetFaceCullMode() const
        { return m_render_attributes.cull_faces; }

    void SetFaceCullMode(FaceCullMode cull_mode)
        { m_render_attributes.cull_faces = cull_mode; }

    MaterialAttributes &GetRenderAttributes()
        { return m_render_attributes; }

    const MaterialAttributes &GetRenderAttributes() const
        { return m_render_attributes; }

    bool IsStatic() const
        { return !m_is_dynamic; }

    bool IsDynamic() const
        { return m_is_dynamic; }

    void Init();
    void Update();

    Handle<Material> Clone() const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_shader.GetHashCode());
        hc.Add(m_parameters.GetHashCode());
        hc.Add(m_textures.GetHashCode());
        hc.Add(m_render_attributes.GetHashCode());

        return hc;
    }

private:
    void EnqueueRenderUpdates();
    void EnqueueTextureUpdate(TextureKey key);
    void EnqueueDescriptorSetCreate();
    void EnqueueDescriptorSetDestroy();

    Handle<Shader>                                      m_shader;

    ParameterTable                                      m_parameters;
    TextureSet                                          m_textures;

    MaterialAttributes                                  m_render_attributes;

    bool                                                m_is_dynamic;

    MaterialShaderData                                  m_shader_data;
    mutable ShaderDataState                             m_shader_data_state;
};

class MaterialGroup : public BasicObject<STUB_CLASS(MaterialGroup)>
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
    FlatMap<HashCode::ValueType, WeakHandle<Material>> m_map;
    std::mutex m_mutex;
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
        to be used from the render thread. If the descriptor set is not found, nullptr
        is returned.

        \param material The IDof material to get the descriptor set for
        \param frame_index The frame index to get the descriptor set for
        \returns The descriptor set for the given material and frame index or nullptr if not found
     */
    const DescriptorSet2Ref &GetDescriptorSet(ID<Material> material, uint frame_index) const;

    /*! \brief Add a material to the manager. This will create a descriptor set for
        the material and add it to the manager. Usable from any thread.

        \param material The ID of the material to add
     */
    void EnqueueAdd(ID<Material> material);

    /*! \brief Add a material to the manager. This will create a descriptor set for
        the material and add it to the manager. Usable from any thread.

        \param material The ID of the material to add
        \param textures The textures to add to the material
     */
    void EnqueueAdd(ID<Material> material, FixedArray<Handle<Texture>, max_bound_textures> &&textures);

    /*! \brief Remove a material from the manager. This will remove the descriptor set
        for the material from the manager. Usable from any thread.

        \param material The ID of the material to remove
     */
    void EnqueueRemove(ID<Material> material);

    void SetNeedsDescriptorSetUpdate(ID<Material> id);

    /*! \brief Initialize the MaterialDescriptorSetManager - Only to be used by the owning Engine instance. */
    void Initialize();

    /*! \brief Update the manager. This will process any pending additions or removals
        and update the descriptor sets. Usable from the render thread.
     */
    void Update(Frame *frame);

private:
    void CreateInvalidMaterialDescriptorSet();

    FlatMap<ID<Material>, FixedArray<DescriptorSet2Ref, max_frames_in_flight>>      m_material_descriptor_sets;

    Array<Pair<ID<Material>, FixedArray<DescriptorSet2Ref, max_frames_in_flight>>>  m_pending_addition;
    Array<ID<Material>>                                                             m_pending_removal;
    Mutex                                                                           m_pending_mutex;
    AtomicVar<bool>                                                                 m_pending_addition_flag;

    FixedArray<Array<ID<Material>>, max_frames_in_flight>                           m_descriptor_sets_to_update;
    Mutex                                                                           m_descriptor_sets_to_update_mutex;
    AtomicVar<uint>                                                                 m_descriptor_sets_to_update_flag;
};

} // namespace hyperion::v2

#endif
