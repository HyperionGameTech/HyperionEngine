#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <rendering/backend/RendererFramebuffer.hpp>

#include <core/utilities/Variant.hpp>

namespace hyperion {

template <class T>
struct Property
{
    void    *parent;
    T       value;

    Property(void *parent)
        : parent(parent)
    {
    }

    template <class... Args>
    Property(void *parent, Args &&... args)
        : parent(parent),
          value(std::forward<Args>(args)...)
    {
    }
};

template <>
class ComponentInterface<MeshComponent> : public ComponentInterfaceBase
{
public:
    virtual ~ComponentInterface() override = default;

protected:
    virtual TypeID GetTypeID_Internal() const override
        { return TypeID::ForType<MeshComponent>(); }

    virtual ANSIStringView GetTypeName_Internal() const override
    {
        static const auto type_name = TypeNameWithoutNamespace<MeshComponent>();

        return type_name.Data();
    }
    
    virtual Array<ComponentProperty> GetProperties_Internal() const override
    {
        return {
            {
                NAME("Mesh"),
                [](const void *component, fbom::FBOMData *out)
                {
                    DebugLog(LogType::Debug, "Get Mesh property, ID = #%u\n", static_cast<const MeshComponent *>(component)->mesh.GetID().Value());

                    fbom::FBOMObject result_object;
                    result_object.SetProperty(NAME("ID"), fbom::FBOMData::FromUnsignedInt(static_cast<const MeshComponent *>(component)->mesh.GetID().Value()));

                    *out = fbom::FBOMData::FromObject(result_object);
                }
            },
            {
                NAME("Flags"),
                [](const void *component, fbom::FBOMData *out)
                {
                    *out = fbom::FBOMData::FromUnsignedInt(uint32(static_cast<const MeshComponent *>(component)->flags));
                },
                [](void *component, const fbom::FBOMData *in)
                {
                    in->ReadUnsignedInt(&static_cast<MeshComponent *>(component)->flags);
                }
            }
        };
    }
};

HYP_REGISTER_COMPONENT_INTERFACE(MeshComponent);

} // namespace hyperion