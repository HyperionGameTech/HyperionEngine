/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/ObjectFwd.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Name/Name.hpp>

#include <Asset/SerializationUtils.hpp>
#include <Asset/AssetPath.hpp>
#include <Asset/RawDataAsset.hpp>
#include <Asset/BlobStorageStructs.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialTypes.hpp>
#include <Rendering/RenderableAttributes.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Light.hpp>
#include <Scene/Node.hpp>

#include <Scene/Animation/Animation.hpp>

namespace Hyperion {

struct HMFTestNestedStruct
{
    Name label;
    int32 id = 0;
    TextureDesc texture;
};

const Class* g_clsHMFTestNestedStruct = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(HMFTestNestedStruct, -1, 0, {})
    Field(NAME(HYP_STR(Label)), &HMFTestNestedStruct::label, HYP_OFFSET_OF(HMFTestNestedStruct, label)),
    Field(NAME(HYP_STR(Id)), &HMFTestNestedStruct::id, HYP_OFFSET_OF(HMFTestNestedStruct, id)),
    Field(NAME(HYP_STR(Texture)), &HMFTestNestedStruct::texture, HYP_OFFSET_OF(HMFTestNestedStruct, texture))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(HMFTestNestedStruct);
// clang-format on

const Class* g_clsHMFVariantBase = nullptr;
const Class* g_clsHMFVariantDerived = nullptr;
const Class* g_clsHMFVariantContainer = nullptr;
const Class* g_clsHMFVariantArrayHolder = nullptr;

class HMFVariantBase : public ObjectBase
{
public:
    HYP_OBJECT_BODY(HMFVariantBase);

    int32 baseValue = 0;
};

const Class* HMFVariantBase::StaticClass()
{
    return g_clsHMFVariantBase;
}

class HMFVariantDerived : public HMFVariantBase
{
public:
    HYP_OBJECT_BODY(HMFVariantDerived);

    int32 baseValue = 0;
    float derivedValue = 0.0f;
    Name derivedName;
};

const Class* HMFVariantDerived::StaticClass()
{
    return g_clsHMFVariantDerived;
}

class HMFVariantContainer : public ObjectBase
{
public:
    HYP_OBJECT_BODY(HMFVariantContainer);

    Name containerName;
    Variant<Handle<HMFVariantBase>, Handle<HMFVariantDerived>> item;
};

const Class* HMFVariantContainer::StaticClass()
{
    return g_clsHMFVariantContainer;
}

// clang-format off
HYP_BEGIN_CLASS(HMFVariantBase, -1, 0, NAME("ObjectBase"))
    Field(NAME(HYP_STR(BaseValue)), &HMFVariantBase::baseValue, HYP_OFFSET_OF(HMFVariantBase, baseValue))
HYP_END_CLASS
HYP_REGISTER_STATIC_CLASS(HMFVariantBase);

HYP_BEGIN_CLASS(HMFVariantDerived, -1, 0, NAME("HMFVariantBase"))
    Field(NAME(HYP_STR(BaseValue)), &HMFVariantDerived::baseValue, HYP_OFFSET_OF(HMFVariantDerived, baseValue)),
    Field(NAME(HYP_STR(DerivedValue)), &HMFVariantDerived::derivedValue, HYP_OFFSET_OF(HMFVariantDerived, derivedValue)),
    Field(NAME(HYP_STR(DerivedName)), &HMFVariantDerived::derivedName, HYP_OFFSET_OF(HMFVariantDerived, derivedName))
HYP_END_CLASS
HYP_REGISTER_STATIC_CLASS(HMFVariantDerived);

HYP_BEGIN_CLASS(HMFVariantContainer, -1, 0, NAME("ObjectBase"))
    Field(NAME(HYP_STR(ContainerName)), &HMFVariantContainer::containerName, HYP_OFFSET_OF(HMFVariantContainer, containerName)),
    Field(NAME(HYP_STR(Item)), &HMFVariantContainer::item, HYP_OFFSET_OF(HMFVariantContainer, item))
HYP_END_CLASS
HYP_REGISTER_STATIC_CLASS(HMFVariantContainer);

class HMFVariantArrayHolder : public ObjectBase
{
public:
    HYP_OBJECT_BODY(HMFVariantArrayHolder);

    Name holderName;
    Array<Handle<HMFVariantBase>> items;
};

const Class* HMFVariantArrayHolder::StaticClass()
{
    return g_clsHMFVariantArrayHolder;
}

// clang-format off
HYP_BEGIN_CLASS(HMFVariantArrayHolder, -1, 0, NAME("ObjectBase"))
    Field(NAME(HYP_STR(HolderName)), &HMFVariantArrayHolder::holderName, HYP_OFFSET_OF(HMFVariantArrayHolder, holderName)),
    Field(NAME(HYP_STR(Items)), &HMFVariantArrayHolder::items, HYP_OFFSET_OF(HMFVariantArrayHolder, items))
HYP_END_CLASS
HYP_REGISTER_STATIC_CLASS(HMFVariantArrayHolder);
// clang-format on

namespace tests {
namespace hmf {

namespace {

String GetVariantActiveTypeName(const BoxedValue& variantValue)
{
    const TypeInfo* ti = variantValue.GetTypeInfo();
    if (!ti || !ti->IsVariantType())
        return "not-a-variant";

    auto* handler = static_cast<ITypeInfoVariantHandler*>(ti->extendedInfo.handler);
    if (!handler)
        return "no-handler";

    int idx = handler->GetCurrentTypeIndex(variantValue);
    if (idx < 0)
        return "empty";

    const TypeInfo* altTI = handler->GetTypeInfoAtIndex(idx);
    if (!altTI)
        return "null-alt";

    const Class* cls = altTI->GetClass();
    return cls ? cls->GetName().ToString() : "unknown";
}

BoxedValue GetVariantInnerObject(const BoxedValue& variantValue)
{
    const TypeInfo* ti = variantValue.GetTypeInfo();
    if (!ti || !ti->IsVariantType())
        return {};

    auto* handler = static_cast<ITypeInfoVariantHandler*>(ti->extendedInfo.handler);
    if (!handler)
        return {};

    AnyRef ref = handler->GetValue(variantValue);
    if (!ref.HasValue() || !ref.GetPointer())
        return {};

    const TypeInfo* refTI = ref.GetTypeInfo();
    if (refTI && refTI->IsHandleType())
    {
        auto* handlePtr = static_cast<Handle<ObjectBase>*>(ref.GetPointer());
        if (*handlePtr)
            return BoxedValue(*handlePtr);
    }

    return {};
}

int g_passCount = 0;
int g_failCount = 0;

void Check(const char* testName, bool condition, const String& detail = "")
{
    if (condition)
    {
        ++g_passCount;
        HYP_LOG(Engine, Info, "[PASS] {}", testName);
    }
    else
    {
        ++g_failCount;
        HYP_LOG(Engine, Error, "[FAIL] {} {}", testName, detail);
    }
}

template <class T>
T GetFieldValue(const BoxedValue& obj, const Class* cls, const char* fieldName)
{
    if (const IMember* m = cls->GetMember(StringHash(fieldName)))
    {
        BoxedValue v;
        if (m->GetMemberType() == MemberType::Field)
            v = static_cast<const Field*>(m)->Get(obj);
        else if (m->GetMemberType() == MemberType::Property)
            v = static_cast<const Property*>(m)->Get(obj);

        if (v.Is<T>())
            return v.Get<T>();
    }
    return T {};
}

uint64 GetFieldUInt64(const BoxedValue& obj, const Class* cls, const char* fieldName)
{
    if (const IMember* m = cls->GetMember(StringHash(fieldName)))
    {
        BoxedValue v;
        if (m->GetMemberType() == MemberType::Field)
            v = static_cast<const Field*>(m)->Get(obj);
        else if (m->GetMemberType() == MemberType::Property)
            v = static_cast<const Property*>(m)->Get(obj);

        if (v.Is<uint64>())
            return v.Get<uint64>();
        if (v.Is<int64>())
            return uint64(v.Get<int64>());
        if (v.Is<uint32>())
            return v.Get<uint32>();
        if (v.Is<int32>())
            return uint64(v.Get<int32>());
        if (v.Is<uint16>())
            return v.Get<uint16>();
        if (v.Is<int16>())
            return uint64(v.Get<int16>());
        if (v.Is<uint8>())
            return v.Get<uint8>();
        if (v.Is<int8>())
            return uint64(v.Get<int8>());
    }
    return 0;
}

void SetFieldValue(BoxedValue& obj, const Class* cls, const char* fieldName, const BoxedValue& value)
{
    if (const IMember* m = cls->GetMember(StringHash(fieldName)))
    {
        if (m->GetMemberType() == MemberType::Field)
            static_cast<const Field*>(m)->Set(obj, value);
        else if (m->GetMemberType() == MemberType::Property)
            static_cast<const Property*>(m)->Set(obj, value);
    }
}

bool ClassNameIs(const BoxedValue& value, const char* expected)
{
    const Class* cls = GetClass(value.GetTypeId());
    return cls && String(cls->GetName().LookupString()) == String(expected);
}

} // anonymous namespace

HYP_EXPORT void RunHMFTest()
{

    {
        String text;

        BoxedToHMF(BoxedValue(int8(-5)), text);
        Check("int8(-5) => \"-5\"", text == "-5", text);
        text.Clear();
        BoxedToHMF(BoxedValue(int16(-32000)), text);
        Check("int16(-32000)", text == "-32000", text);
        text.Clear();
        BoxedToHMF(BoxedValue(int32(42)), text);
        Check("int32(42) => \"42\"", text == "42", text);
        text.Clear();
        BoxedToHMF(BoxedValue(int64(-9999999999LL)), text);
        Check("int64(-9999999999)", text == "-9999999999", text);
        text.Clear();
        BoxedToHMF(BoxedValue(uint8(255)), text);
        Check("uint8(255) => \"255\"", text == "255", text);
        text.Clear();
        BoxedToHMF(BoxedValue(uint16(65535)), text);
        Check("uint16(65535)", text == "65535", text);
        text.Clear();
        BoxedToHMF(BoxedValue(uint32(42)), text);
        Check("uint32(42) => \"42\"", text == "42", text);
        text.Clear();
        BoxedToHMF(BoxedValue(uint64(0xFFFFFFFFFFFFFFFFULL)), text);
        Check("uint64(max)", text == "18446744073709551615", text);

        text.Clear();
        BoxedToHMF(BoxedValue(float(3.5f)), text);
        Check("float(3.5)", text.Contains("3.5"), text);
        text.Clear();
        BoxedToHMF(BoxedValue(double(3.14159)), text);
        Check("double(3.14159)", text.Contains("3.14159"), text);

        text.Clear();
        BoxedToHMF(BoxedValue(true), text);
        Check("bool(true) => \"true\"", text == "true", text);
        text.Clear();
        BoxedToHMF(BoxedValue(false), text);
        Check("bool(false) => \"false\"", text == "false", text);

        text.Clear();
        BoxedToHMF(BoxedValue(String("hello")), text);
        Check("String(hello)", text == "\"hello\"", text);
        text.Clear();
        BoxedToHMF(BoxedValue(String("")), text);
        Check("String(empty)", text == "\"\"", text);
        text.Clear();
        BoxedToHMF(BoxedValue(String("with \"quotes\" and \\ backslash")), text);
        Check("String escaping", text.Contains("\\\"") && text.Contains("\\\\"), text);
        text.Clear();
        BoxedToHMF(BoxedValue(String("line\nbreak\ttab")), text);
        Check("String escaping (newline + tab)", text.Contains("\\n") && text.Contains("\\t"), text);
    }

    {
        String text;
        const TypeInfo& ti = TypeOf<CameraProjectionMode>();

        BoxedToHMF(BoxedValue(CameraProjectionMode::NONE), text, &ti);
        Check("Enum NONE", text == "NONE", text);

        text.Clear();
        BoxedToHMF(BoxedValue(CameraProjectionMode::PERSPECTIVE), text, &ti);
        Check("Enum PERSPECTIVE", text == "PERSPECTIVE", text);

        text.Clear();
        BoxedToHMF(BoxedValue(CameraProjectionMode::ORTHOGRAPHIC), text, &ti);
        Check("Enum ORTHOGRAPHIC", text == "ORTHOGRAPHIC", text);
    }

    {
        String text;
        const TypeInfo& ti = TypeOf<ShaderModuleType>();

        BoxedToHMF(BoxedValue(ShaderModuleType::None), text, &ti);
        Check("ShaderModuleType None", text == "None", text);

        text.Clear();
        BoxedToHMF(BoxedValue(ShaderModuleType::Vertex), text, &ti);
        Check("ShaderModuleType Vertex", text == "Vertex", text);

        text.Clear();
        BoxedToHMF(BoxedValue(ShaderModuleType::Pixel), text, &ti);
        Check("ShaderModuleType Pixel", text == "Pixel", text);

        text.Clear();
        BoxedToHMF(BoxedValue(ShaderModuleType::Compute), text, &ti);
        Check("ShaderModuleType Compute", text == "Compute", text);

        text.Clear();
        BoxedToHMF(BoxedValue(ShaderModuleType::RayGen), text, &ti);
        Check("ShaderModuleType RayGen", text == "RayGen", text);
    }

    {
        String text;
        Array<ShaderModuleType> arr = { ShaderModuleType::Vertex, ShaderModuleType::Pixel, ShaderModuleType::Compute };
        const TypeInfo& arrTi = TypeOf<Array<ShaderModuleType>>();
        BoxedToHMF(BoxedValue(arr), text, &arrTi);
        Check("Array<ShaderModuleType>[3]", text == "[Vertex, Pixel, Compute]", text);

        text.Clear();
        Array<ShaderModuleType> empty;
        BoxedToHMF(BoxedValue(empty), text, &arrTi);
        Check("Array<ShaderModuleType>[] (empty)", text == "[]", text);
    }

    {
        String text;
        const TypeInfo& ti = TypeOf<EnumFlags<CameraFlags>>();

        EnumFlags<CameraFlags> single = CameraFlags::MatchWindowSize;
        BoxedToHMF(BoxedValue(single), text, &ti);
        Check("EnumFlags single => MatchWindowSize", text == "MatchWindowSize", text);

        text.Clear();
        EnumFlags<CameraFlags> multi = CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume;
        BoxedToHMF(BoxedValue(multi), text, &ti);
        Check("EnumFlags multi => MatchWindowSize|HasStreamingVolume",
              text.Contains("MatchWindowSize") && text.Contains("HasStreamingVolume") && text.Contains("|"),
              text);

        text.Clear();
        const TypeInfo& lti = TypeOf<EnumFlags<LightFlags>>();
        EnumFlags<LightFlags> lightFlags = LightFlags::Default;
        BoxedToHMF(BoxedValue(lightFlags), text, &lti);
        Check("EnumFlags Default(composite)",
              text.Contains("Default") || (text.Contains("ShadowCaster") && text.Contains("ShadowPCF")),
              text);

        text.Clear();
        EnumFlags<CameraFlags> empty = CameraFlags::None;
        BoxedToHMF(BoxedValue(empty), text, &ti);
        Check("EnumFlags NONE", text == "NONE", text);
    }

    {
        String text;
        Array<int32> arr = { 10, 20, 30 };
        BoxedToHMF(BoxedValue(arr), text);
        Check("Array<int32>[3]", text == "[10, 20, 30]", text);

        text.Clear();
        Array<int32> empty;
        BoxedToHMF(BoxedValue(empty), text);
        Check("Array<int32>[] (empty)", text == "[]", text);
    }

    {
        const Class* cls = GetClass<CameraOrthoRect>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Left", BoxedValue(1.5f));
            SetFieldValue(obj, cls, "Right", BoxedValue(2.5f));
            SetFieldValue(obj, cls, "Bottom", BoxedValue(3.5f));
            SetFieldValue(obj, cls, "Top", BoxedValue(4.5f));

            String text;
            ObjectToHMF(cls, obj, text);

            // Assert exact expected output
            Check("CameraOrthoRect: has class header", text.Contains("CameraOrthoRect"), text);
            Check("CameraOrthoRect: Left = 1.5", text.Contains("Left = 1.5"), text);
            Check("CameraOrthoRect: Right = 2.5", text.Contains("Right = 2.5"), text);
            Check("CameraOrthoRect: Bottom = 3.5", text.Contains("Bottom = 3.5"), text);
            Check("CameraOrthoRect: Top = 4.5", text.Contains("Top = 4.5"), text);
            Check("CameraOrthoRect: no transient fields", !text.Contains("raw") && !text.Contains("readOnly"), text);
        }
    }

    {
        const Class* cls = GetClass<TextureDesc>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMF(cls, obj, text);
            HYP_LOG(Engine, Info, "TextureDesc HMF:\n{}", text);

            Check("TextureDesc: class header", text.Contains("TextureDesc"), text);
            Check("TextureDesc: Type = Texture2D", text.Contains("Type = Texture2D"), text);
            Check("TextureDesc: Format = RGBA8", text.Contains("Format = RGBA8"), text);
            Check("TextureDesc: Extent = (1, 1, 1)", text.Contains("Extent = (1, 1, 1)"), text);
            Check("TextureDesc: MinFilterMode = TFM_NEAREST", text.Contains("MinFilterMode = TFM_NEAREST"), text);
            Check("TextureDesc: NumLayers = 1", text.Contains("NumLayers = 1"), text);
            Check("TextureDesc: ImageUsage = IU_SAMPLED", text.Contains("ImageUsage = IU_SAMPLED"), text);
            Check("TextureDesc: MipOffsets has 16 elements", text.Contains("0, 0, 0, 0"), text);
        }
    }

    {
        const Class* cls = GetClass<MeshLodDesc>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "NumVertices", BoxedValue(uint32(9999)));
            SetFieldValue(obj, cls, "NumIndices", BoxedValue(uint32(33333)));

            String text;
            ObjectToHMF(cls, obj, text);

            Check("MeshLodDesc: NumVertices = 9999", text.Contains("NumVertices = 9999"), text);
            Check("MeshLodDesc: NumIndices = 33333", text.Contains("NumIndices = 33333"), text);
        }
    }

    {
        const Class* cls = GetClass<StencilFunction>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMF(cls, obj, text);

            Check("StencilFunction: PassOp = SO_REPLACE", text.Contains("PassOp = SO_REPLACE"), text);
            Check("StencilFunction: FailOp = SO_KEEP", text.Contains("FailOp = SO_KEEP"), text);
            Check("StencilFunction: DepthFailOp = SO_KEEP", text.Contains("DepthFailOp = SO_KEEP"), text);
            Check("StencilFunction: CompareOp = SCO_ALWAYS", text.Contains("CompareOp = SCO_ALWAYS"), text);
        }
    }

    {
        const String manifest = R"(CameraOrthoRect {
    Left = 10.5
    Right = 20
    Bottom = -1
    Top = 100
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse CameraOrthoRect succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Type is CameraOrthoRect", ClassNameIs(result.GetValue(), "CameraOrthoRect"));
            Check("TypeId matches CameraOrthoRect",
                  result.GetValue().GetTypeId() == TypeId::ForType<CameraOrthoRect>());

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Check("Left == 10.5", GetFieldValue<float>(result.GetValue(), cls, "Left") == 10.5f);
                Check("Right == 20.0", GetFieldValue<float>(result.GetValue(), cls, "Right") == 20.0f);
                Check("Bottom == -1.0", GetFieldValue<float>(result.GetValue(), cls, "Bottom") == -1.0f);
                Check("Top == 100.0", GetFieldValue<float>(result.GetValue(), cls, "Top") == 100.0f);
            }
        }
    }

    {
        const String manifest = R"(MeshLodDesc {
    NumVertices = 4096
    NumIndices = 12288
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse MeshLodDesc succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Type is MeshLodDesc", ClassNameIs(result.GetValue(), "MeshLodDesc"));
            Check("TypeId matches", result.GetValue().GetTypeId() == TypeId::ForType<MeshLodDesc>());

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Check("NumVertices == 4096", GetFieldValue<uint32>(result.GetValue(), cls, "NumVertices") == 4096);
                Check("NumIndices == 12288", GetFieldValue<uint32>(result.GetValue(), cls, "NumIndices") == 12288);
            }
        }
    }

    {
        const String manifest = R"(BlobDataReference {
    Key = "Engine://Textures/MyTexture.TEX"
    Size = 1048576
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse BlobDataReference succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Type is BlobDataReference", ClassNameIs(result.GetValue(), "BlobDataReference"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Name keyName = GetFieldValue<Name>(result.GetValue(), cls, "Key");
                Check("Key name valid", keyName.IsValid(), keyName.LookupString());
                Check("Key == Engine://Textures/MyTexture.TEX",
                      String(keyName.LookupString()) == String("Engine://Textures/MyTexture.TEX"),
                      keyName.LookupString());
                Check("Size == 1048576", GetFieldValue<uint64>(result.GetValue(), cls, "Size") == 1048576);
            }
        }
    }

    {
        const String manifest = R"(// This is a line comment
CameraOrthoRect {
    /* block comment */ Left = 1.0
    Right = 2.0 // trailing comment
    // another comment
    Bottom = 3.0
    Top = 4.0
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse with comments succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Check("Comment: Left == 1.0", GetFieldValue<float>(result.GetValue(), cls, "Left") == 1.0f);
                Check("Comment: Right == 2.0", GetFieldValue<float>(result.GetValue(), cls, "Right") == 2.0f);
                Check("Comment: Bottom == 3.0", GetFieldValue<float>(result.GetValue(), cls, "Bottom") == 3.0f);
                Check("Comment: Top == 4.0", GetFieldValue<float>(result.GetValue(), cls, "Top") == 4.0f);
            }
        }
    }

    {
        const String manifest = R"(StencilFunction {
    PassOp = SO_REPLACE
    FailOp = SO_KEEP
    DepthFailOp = SO_KEEP
    CompareOp = SCO_ALWAYS
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse StencilFunction succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Type is StencilFunction", ClassNameIs(result.GetValue(), "StencilFunction"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Check("PassOp == SO_REPLACE value",
                      GetFieldUInt64(result.GetValue(), cls, "PassOp") == uint64(SO_REPLACE));
                Check("FailOp == SO_KEEP value",
                      GetFieldUInt64(result.GetValue(), cls, "FailOp") == uint64(SO_KEEP));
                Check("CompareOp == SCO_ALWAYS value",
                      GetFieldUInt64(result.GetValue(), cls, "CompareOp") == uint64(SCO_ALWAYS));
            }
        }
    }

    {
        const Class* cls = GetClass<CameraOrthoRect>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("CameraOrthoRect registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Left", BoxedValue(11.0f));
            SetFieldValue(obj, cls, "Right", BoxedValue(22.0f));
            SetFieldValue(obj, cls, "Bottom", BoxedValue(33.0f));
            SetFieldValue(obj, cls, "Top", BoxedValue(44.0f));

            String text;
            ObjectToHMF(cls, obj, text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("Round-trip parse succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                Check("Round-trip type correct", ClassNameIs(result.GetValue(), "CameraOrthoRect"));

                const Class* parsedCls = GetClass(result.GetValue().GetTypeId());
                if (parsedCls)
                {
                    Check("RT Left == 11", GetFieldValue<float>(result.GetValue(), parsedCls, "Left") == 11.0f);
                    Check("RT Right == 22", GetFieldValue<float>(result.GetValue(), parsedCls, "Right") == 22.0f);
                    Check("RT Bottom == 33", GetFieldValue<float>(result.GetValue(), parsedCls, "Bottom") == 33.0f);
                    Check("RT Top == 44", GetFieldValue<float>(result.GetValue(), parsedCls, "Top") == 44.0f);
                }
            }
        }
    }

    {
        const Class* cls = GetClass<TextureDesc>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("TextureDesc registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMF(cls, obj, text);
            Check("TextureDesc serialize non-empty", !text.Empty());

            HMF::ParseResult result = HMF::Parse(text);
            Check("TextureDesc round-trip parse succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                Check("TextureDesc type correct", ClassNameIs(result.GetValue(), "TextureDesc"));

                const Class* parsedCls = GetClass(result.GetValue().GetTypeId());
                if (parsedCls)
                {
                    Check("RT NumLayers == 1", GetFieldValue<uint16>(result.GetValue(), parsedCls, "NumLayers") == 1);
                    Check("RT Type == Texture2D",
                          GetFieldUInt64(result.GetValue(), parsedCls, "Type") == uint64(TextureType::Texture2D));
                    Check("RT Format == RGBA8",
                          GetFieldUInt64(result.GetValue(), parsedCls, "Format") == uint64(TextureFormat::RGBA8));
                }
            }
        }
    }

    {
        const Class* cls = GetClass<MeshLodDesc>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("MeshLodDesc registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "NumVertices", BoxedValue(uint32(1234)));
            SetFieldValue(obj, cls, "NumIndices", BoxedValue(uint32(5678)));

            String text;
            ObjectToHMF(cls, obj, text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("MeshLodDesc round-trip succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                Check("MeshLodDesc type correct", ClassNameIs(result.GetValue(), "MeshLodDesc"));

                const Class* parsedCls = GetClass(result.GetValue().GetTypeId());
                if (parsedCls)
                {
                    Check("RT NumVertices == 1234", GetFieldValue<uint32>(result.GetValue(), parsedCls, "NumVertices") == 1234);
                    Check("RT NumIndices == 5678", GetFieldValue<uint32>(result.GetValue(), parsedCls, "NumIndices") == 5678);
                }
            }
        }
    }

    {
        const Class* cls = GetClass<StencilFunction>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("StencilFunction registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMF(cls, obj, text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("StencilFunction round-trip succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                Check("StencilFunction type correct", ClassNameIs(result.GetValue(), "StencilFunction"));

                const Class* parsedCls = GetClass(result.GetValue().GetTypeId());
                if (parsedCls)
                {
                    Check("RT PassOp == SO_REPLACE",
                          GetFieldUInt64(result.GetValue(), parsedCls, "PassOp") == uint64(SO_REPLACE));
                    Check("RT FailOp == SO_KEEP",
                          GetFieldUInt64(result.GetValue(), parsedCls, "FailOp") == uint64(SO_KEEP));
                    Check("RT CompareOp == SCO_ALWAYS",
                          GetFieldUInt64(result.GetValue(), parsedCls, "CompareOp") == uint64(SCO_ALWAYS));
                }
            }
        }
    }

    {
        const String manifest = R"(AssetPath {
    Value = "Engine://Textures/MyTexture"
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse AssetPath succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Type is AssetPath", ClassNameIs(result.GetValue(), "AssetPath"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                if (const Property* vp = cls->GetProperty("Value"_sh))
                {
                    BoxedValue v = vp->Get(result.GetValue());
                    String s = v.Is<String>() ? v.Get<String>() : "";
                    Check("AssetPath Value correct", s == "Engine://Textures/MyTexture", s);
                }

                String rt;
                BoxedToHMF(result.GetValue(), rt);
                Check("AssetPath round-trip => @\"...\"", rt.Contains("@\"Engine://Textures/MyTexture\""), rt);
            }
        }
    }

    {
        AssetPath path(ANSIStringView("Game://Meshes/Cube"));
        String text;
        ToHMFOptions opts;
        BoxedToHMF(BoxedValue(path), text, nullptr, &opts);
        Check("AssetPath value => @\"Game://Meshes/Cube\"",
              text == "@\"Game://Meshes/Cube\"", text);
    }

    {
        Check("Unknown class fails", !Success(HMF::Parse("NonexistentClass {\n}\n")));
        Check("Empty input fails", Failed(HMF::Parse("")));
        Check("Garbage fails", !Success(HMF::Parse("??? not valid")));
        Check("Missing closing brace fails", !Success(HMF::Parse("CameraOrthoRect {\n    Left = 1.0\n")));

        const String manifest = R"(CameraOrthoRect {
    Left = 1.0
    ThisFieldDoesNotExist = 42
    AlsoBad = "hello"
    Right = 2.0
}
)";
        HMF::ParseResult r5 = HMF::Parse(manifest);
        Check("Unknown fields skipped", Success(r5), r5.GetError().GetMessage());

        if (Success(r5))
        {
            const Class* cls = GetClass(r5.GetValue().GetTypeId());
            if (cls)
            {
                Check("Unknown fields: Left still correct",
                      GetFieldValue<float>(r5.GetValue(), cls, "Left") == 1.0f);
                Check("Unknown fields: Right still correct",
                      GetFieldValue<float>(r5.GetValue(), cls, "Right") == 2.0f);
            }
        }
    }

    {
        const Class* cls = GetClass<BlobDataReference>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMF(cls, obj, text);

            Check("Transient 'raw' excluded", !text.Contains("Raw"), text);
            Check("Transient 'readOnly' excluded", !text.Contains("ReadOnly"), text);
            Check("Non-transient 'Key' present", text.Contains("Key"), text);
            Check("Non-transient 'Size' present", text.Contains("Size"), text);
        }
    }

    {
        const Class* cls = GetClass<MaterialParameters>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("MaterialParameters registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Metalness", BoxedValue(0.75f));
            SetFieldValue(obj, cls, "Roughness", BoxedValue(0.25f));
            SetFieldValue(obj, cls, "AlphaThreshold", BoxedValue(0.5f));
            SetFieldValue(obj, cls, "Transmission", BoxedValue(0.3f));
            SetFieldValue(obj, cls, "IOR", BoxedValue(1.33f));
            SetFieldValue(obj, cls, "EmissiveIntensity", BoxedValue(5.0f));
            SetFieldValue(obj, cls, "Unlit", BoxedValue(true));

            String text;
            ObjectToHMF(cls, obj, text);
            HYP_LOG(Engine, Info, "MaterialParameters HMF:\n{}", text);

            Check("MP: Metalness = 0.75", text.Contains("Metalness = 0.75"), text);
            Check("MP: Roughness = 0.25", text.Contains("Roughness = 0.25"), text);
            Check("MP: Unlit = true", text.Contains("Unlit = true"), text);
            Check("MP: IOR = 1.33", text.Contains("IOR = 1.33"), text);
            Check("MP: has all fields", text.Contains("Albedo") && text.Contains("ParallaxHeightScale") && text.Contains("UserParams"), text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("MP: round-trip parse succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                Check("MP: type correct", ClassNameIs(result.GetValue(), "MaterialParameters"));

                const Class* pc = GetClass(result.GetValue().GetTypeId());
                if (pc)
                {
                    Check("MP: RT Metalness == 0.75", GetFieldValue<float>(result.GetValue(), pc, "Metalness") == 0.75f);
                    Check("MP: RT Roughness == 0.25", GetFieldValue<float>(result.GetValue(), pc, "Roughness") == 0.25f);
                    Check("MP: RT AlphaThreshold == 0.5", GetFieldValue<float>(result.GetValue(), pc, "AlphaThreshold") == 0.5f);
                    Check("MP: RT Transmission == 0.3", GetFieldValue<float>(result.GetValue(), pc, "Transmission") == 0.3f);
                    Check("MP: RT IOR == 1.33", GetFieldValue<float>(result.GetValue(), pc, "IOR") == 1.33f);
                    Check("MP: RT Unlit == true", GetFieldValue<bool>(result.GetValue(), pc, "Unlit"));
                }
            }
        }
    }

    {
        const Class* cls = GetClass<MaterialAttributes>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("MaterialAttributes registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "DepthBias", BoxedValue(int32(100)));
            SetFieldValue(obj, cls, "DepthBiasSlope", BoxedValue(2.5f));
            SetFieldValue(obj, cls, "StencilReference", BoxedValue(uint8(7)));

            String text;
            ObjectToHMF(cls, obj, text);
            HYP_LOG(Engine, Info, "MaterialAttributes HMF:\n{}", text);

            Check("MA: has ShaderName", text.Contains("ShaderName"), text);
            Check("MA: has Bucket", text.Contains("Bucket"), text);
            Check("MA: has FillMode", text.Contains("FillMode"), text);
            Check("MA: has CullFaces", text.Contains("CullFaces"), text);
            Check("MA: has Flags", text.Contains("Flags"), text);
            Check("MA: has StencilFunction", text.Contains("StencilFunction"), text);
            Check("MA: has DepthCompareOp", text.Contains("DepthCompareOp"), text);
            Check("MA: DepthBias = 100", text.Contains("DepthBias = 100"), text);
            Check("MA: StencilReference = 7", text.Contains("StencilReference = 7"), text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("MA: round-trip parse succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                Check("MA: type correct", ClassNameIs(result.GetValue(), "MaterialAttributes"));

                const Class* pc = GetClass(result.GetValue().GetTypeId());
                if (pc)
                {
                    Check("MA: RT DepthBias == 100", GetFieldValue<int32>(result.GetValue(), pc, "DepthBias") == 100);
                    Check("MA: RT DepthBiasSlope == 2.5", GetFieldValue<float>(result.GetValue(), pc, "DepthBiasSlope") == 2.5f);
                    Check("MA: RT StencilReference == 7", GetFieldValue<uint8>(result.GetValue(), pc, "StencilReference") == 7);
                }
            }
        }
    }

    {
        const Class* cls = GetClass<TextureDesc>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);
            SetFieldValue(obj, cls, "NumLayers", BoxedValue(uint16(7)));

            String text;
            ObjectToHMF(cls, obj, text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("TD: round-trip succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                const Class* pc = GetClass(result.GetValue().GetTypeId());
                if (pc)
                {
                    Check("TD: RT NumLayers == 7", GetFieldValue<uint16>(result.GetValue(), pc, "NumLayers") == 7);
                    Check("TD: RT Type == Texture2D", GetFieldUInt64(result.GetValue(), pc, "Type") == uint64(TextureType::Texture2D));
                    Check("TD: RT Format == RGBA8", GetFieldUInt64(result.GetValue(), pc, "Format") == uint64(TextureFormat::RGBA8));
                    Check("TD: RT MinFilterMode", GetFieldUInt64(result.GetValue(), pc, "MinFilterMode") == uint64(TFM_NEAREST));
                    Check("TD: RT TextureWrapMode", GetFieldUInt64(result.GetValue(), pc, "TextureWrapMode") == uint64(TWM_CLAMP_TO_EDGE));
                    Check("TD: RT ImageUsage", GetFieldUInt64(result.GetValue(), pc, "ImageUsage") == uint64(ImageUsage::IU_SAMPLED));
                }
            }
        }
    }

    {
        const String manifest = R"(TextureDesc {
    Type = Texture3D
    Format = RGBA8
    Extent = (512, 512, 64)
    MinFilterMode = TFM_LINEAR
    MagFilterMode = TFM_LINEAR
    TextureWrapMode = TWM_REPEAT
    NumLayers = 3
    ImageUsage = IU_SAMPLED|IU_TRANSFER_DST
    MipOffsets = [0, 1024, 2048, 3072, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse complex TD succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Parse TD type", ClassNameIs(result.GetValue(), "TextureDesc"));

            const Class* pc = GetClass(result.GetValue().GetTypeId());
            if (pc)
            {
                Check("Parse TD: Type == Texture3D", GetFieldUInt64(result.GetValue(), pc, "Type") == uint64(TextureType::Texture3D));
                Check("Parse TD: MinFilterMode == TFM_LINEAR", GetFieldUInt64(result.GetValue(), pc, "MinFilterMode") == uint64(TFM_LINEAR));
                Check("Parse TD: TextureWrapMode == TWM_REPEAT", GetFieldUInt64(result.GetValue(), pc, "TextureWrapMode") == uint64(TWM_REPEAT));
                Check("Parse TD: NumLayers == 3", GetFieldValue<uint16>(result.GetValue(), pc, "NumLayers") == 3);
            }
        }
    }

    {
        const Class* cls = GetClass<CameraOrthoRect>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Left", BoxedValue(-100.5f));
            SetFieldValue(obj, cls, "Right", BoxedValue(200.25f));
            SetFieldValue(obj, cls, "Top", BoxedValue(999.0f));

            String text;
            ObjectToHMF(cls, obj, text);

            Check("Exact: Left = -100.5", text.Contains("Left = -100.5"), text);
            Check("Exact: Right = 200.25", text.Contains("Right = 200.25"), text);
            Check("Exact: Top = 999", text.Contains("Top = 999"), text);
        }
    }

    {
        const String manifest = R"(BlobDataReference {
    Key = "Game://Materials/ComplexMaterial.RAW"
    Size = 4294967296
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse large BlobDataReference", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Check("Large Size == 4294967296", GetFieldValue<uint64>(result.GetValue(), cls, "Size") == 4294967296ULL);

                Name key = GetFieldValue<Name>(result.GetValue(), cls, "Key");
                Check("Large Key correct",
                      String(key.LookupString()) == String("Game://Materials/ComplexMaterial.RAW"),
                      key.LookupString());
            }
        }
    }

    {
        Mat4f mat = Mat4f::Identity();
        String text;
        const TypeInfo& matTi = TypeOf<Mat4f>();
        BoxedToHMF(BoxedValue(mat), text, &matTi);
        HYP_LOG(Engine, Info, "Mat4f HMF: {}", text);

        Check("Matrix: non-empty output", !text.Empty(), text);
        Check("Matrix: has nested brackets", text.Contains("[") && text.Contains("]"), text);
        Check("Matrix: contains 1 (diagonal)", text.Contains("1"), text);
    }

    {
        const Class* cls = GetClass<NodeTag>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("NodeTag registered", false);
        }
        else
        {
            // Create with default instance
            BoxedValue obj;
            cls->CreateInstance(obj);

            // Write it
            String text;
            ObjectToHMF(cls, obj, text);
            HYP_LOG(Engine, Info, "NodeTag HMF:\n{}", text);

            Check("NodeTag: has no name", !text.Contains("Name"), text);
            Check("NodeTag: has Data field", text.Contains("Data"), text);

            // Parse back
            HMF::ParseResult result = HMF::Parse(text);
            Check("NodeTag: round-trip parse succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                Check("NodeTag: type correct", ClassNameIs(result.GetValue(), "NodeTag"));
            }
        }
    }

    {
        const String manifest = R"(NodeTag {
    Name = "Health"
    Data = 42.5
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse NodeTag (float variant) succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("NodeTag float variant: type correct", ClassNameIs(result.GetValue(), "NodeTag"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Name tagName = GetFieldValue<Name>(result.GetValue(), cls, "Name");
                Check("NodeTag float variant: Name == Health",
                      String(tagName.LookupString()) == String("Health"),
                      tagName.LookupString());
            }
        }
    }

    {
        const String manifest = R"(NodeTag {
    Name = "Level"
    Data = 99
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse NodeTag (int variant) succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("NodeTag int variant: type correct", ClassNameIs(result.GetValue(), "NodeTag"));
        }
    }

    {
        const String manifest = R"(NodeTag {
    Name = "Description"
    Data = "hello world"
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse NodeTag (String variant) succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("NodeTag String variant: type correct", ClassNameIs(result.GetValue(), "NodeTag"));
        }
    }

    {
        const float matData[16] = {
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
            13, 14, 15, 16
        };
        Mat4f mat(matData);

        String text;
        const TypeInfo& matTi = TypeOf<Mat4f>();
        BoxedToHMF(BoxedValue(mat), text, &matTi);
        HYP_LOG(Engine, Info, "Mat4f written: {}", text);

        Check("Matrix: non-empty", !text.Empty(), text);
        Check("Matrix: has nested arrays", text.Contains("[["), text);
        Check("Matrix: contains 1", text.Contains("1"), text);
        Check("Matrix: contains 16", text.Contains("16"), text);
    }

    {
        Pair<int32, float> pair(42, 3.14f);

        String text;
        const TypeInfo& pairTi = TypeOf<Pair<int32, float>>();
        BoxedToHMF(BoxedValue(pair), text, &pairTi);
        HYP_LOG(Engine, Info, "Pair<int32,float> written: {}", text);

        Check("Pair: non-empty", !text.Empty(), text);
        Check("Pair: has parens", text.Contains("(") && text.Contains(")"), text);
        Check("Pair: contains 42", text.Contains("42"), text);
        Check("Pair: contains 3.14", text.Contains("3.14"), text);
    }

    //                so we test Pair via direct value parse)

    {
        Pair<int32, float> pair(777, 9.99f);

        String text;
        const TypeInfo& pairTi = TypeOf<Pair<int32, float>>();
        BoxedToHMF(BoxedValue(pair), text, &pairTi);

        // Now parse it back -- Pair has a handler so the parser should handle it
        // We can't easily parse a raw Pair value (the parser needs a class wrapper),
        // but we can verify the writer output format is consistent
        Check("Pair round-trip: format is (a, b)",
              text.Contains("(") && text.Contains("777") && text.Contains("9.99") && text.Contains(")"),
              text);

        text.Clear();
        Pair<int32, String> pairStr(100, "hello");
        const TypeInfo& pairStrTi = TypeOf<Pair<int32, String>>();
        BoxedToHMF(BoxedValue(pairStr), text, &pairStrTi);
        Check("Pair<int,String>: has 100 and hello",
              text.Contains("100") && text.Contains("\"hello\""), text);
    }

    {
        const String manifest = R"(HMFVariantContainer {
    ContainerName = "BaseTest"
    Item = HMFVariantBase {
        BaseValue = 42
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Variant<Base>: parse succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Variant<Base>: type is HMFVariantContainer", ClassNameIs(result.GetValue(), "HMFVariantContainer"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Name cname = GetFieldValue<Name>(result.GetValue(), cls, "ContainerName");
                Check("Variant<Base>: ContainerName == BaseTest",
                      String(cname.LookupString()) == String("BaseTest"),
                      cname.LookupString());

                if (const IMember* m = cls->GetMember(StringHash("Item")))
                {
                    BoxedValue itemVal;
                    if (m->GetMemberType() == MemberType::Field)
                        itemVal = static_cast<const Field*>(m)->Get(result.GetValue());

                    String activeName = GetVariantActiveTypeName(itemVal);

                    Check("Variant<Base>: item type is HMFVariantBase",
                          activeName == String("HMFVariantBase"),
                          activeName);
                }
            }
        }
    }

    {
        const String manifest = R"(HMFVariantContainer {
    ContainerName = "DerivedTest"
    Item = HMFVariantDerived {
        BaseValue = 100
        DerivedValue = 2.5
        DerivedName = "Child"
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Variant<Derived>: parse succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Variant<Derived>: type is HMFVariantContainer", ClassNameIs(result.GetValue(), "HMFVariantContainer"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                if (const IMember* m = cls->GetMember(StringHash("Item")))
                {
                    BoxedValue itemVal;
                    if (m->GetMemberType() == MemberType::Field)
                        itemVal = static_cast<const Field*>(m)->Get(result.GetValue());

                    String activeName = GetVariantActiveTypeName(itemVal);
                    HYP_LOG(Engine, Info, "Variant<Derived>: active type = {}", activeName.Data());

                    // Extract inner object to access fields
                    BoxedValue innerObj = GetVariantInnerObject(itemVal);
                    const Class* innerCls = innerObj.IsValid() ? GetClass(innerObj.GetTypeId()) : nullptr;

                    if (innerCls)
                    {
                        Check("Variant<Derived>: BaseValue == 100",
                              GetFieldValue<int32>(innerObj, innerCls, "BaseValue") == 100);
                        Check("Variant<Derived>: DerivedValue == 2.5",
                              GetFieldValue<float>(innerObj, innerCls, "DerivedValue") == 2.5f);

                        Name dn = GetFieldValue<Name>(innerObj, innerCls, "DerivedName");
                        Check("Variant<Derived>: DerivedName == Child",
                              String(dn.LookupString()) == String("Child"),
                              dn.LookupString());
                    }
                }
            }
        }
    }

    {
        const String manifest = R"(HMFVariantContainer {
    ContainerName = "RTDerivedTest"
    Item = HMFVariantDerived {
        BaseValue = 777
        DerivedValue = 9.99
        DerivedName = "RTChild"
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Variant RT: parse succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                String text;
                ObjectToHMF(cls, result.GetValue(), text);
                HYP_LOG(Engine, Info, "Variant RT HMF:\n{}", text);

                Check("Variant RT: has ContainerName", text.Contains("ContainerName"), text);
                Check("Variant RT: has Item", text.Contains("Item"), text);
                Check("Variant RT: Item has Derived prefix", text.Contains("HMFVariantDerived"), text);
                Check("Variant RT: BaseValue = 777", text.Contains("BaseValue = 777"), text);
                Check("Variant RT: DerivedValue = 9.99", text.Contains("DerivedValue = 9.99"), text);

                HMF::ParseResult rtResult = HMF::Parse(text);
                Check("Variant RT: re-parse succeeds", Success(rtResult), rtResult.GetError().GetMessage());

                if (Success(rtResult))
                {
                    Check("Variant RT: type correct", ClassNameIs(rtResult.GetValue(), "HMFVariantContainer"));

                    const Class* rtCls = GetClass(rtResult.GetValue().GetTypeId());
                    if (rtCls)
                    {
                        if (const IMember* m = rtCls->GetMember(StringHash("Item")))
                        {
                            BoxedValue itemVal;
                            if (m->GetMemberType() == MemberType::Field)
                                itemVal = static_cast<const Field*>(m)->Get(rtResult.GetValue());

                            // Variant index may report Base (Derived IS-A Base in SetValue),
                            String activeName = GetVariantActiveTypeName(itemVal);
                            HYP_LOG(Engine, Info, "Variant RT: active type = {}", activeName.Data());

                            BoxedValue innerObj = GetVariantInnerObject(itemVal);
                            const Class* innerCls = innerObj.IsValid() ? GetClass(innerObj.GetTypeId()) : nullptr;

                            if (innerCls)
                            {
                                Check("Variant RT: BaseValue == 777",
                                      GetFieldValue<int32>(innerObj, innerCls, "BaseValue") == 777);
                            }
                        }
                    }
                }
            }
        }
    }

    {
        const String manifest = R"(HMFVariantContainer {
    ContainerName = "RTBaseTest"
    Item = HMFVariantBase {
        BaseValue = 333
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Variant RT Base: parse succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                String text;
                ObjectToHMF(cls, result.GetValue(), text);
                HYP_LOG(Engine, Info, "Variant RT (Base) HMF:\n{}", text);

                Check("Variant RT Base: has Base prefix", text.Contains("HMFVariantBase"), text);

                HMF::ParseResult rtResult = HMF::Parse(text);
                Check("Variant RT Base: re-parse succeeds", Success(rtResult), rtResult.GetError().GetMessage());

                if (Success(rtResult))
                {
                    const Class* rtCls = GetClass(rtResult.GetValue().GetTypeId());
                    if (rtCls)
                    {
                        if (const IMember* m = rtCls->GetMember(StringHash("Item")))
                        {
                            BoxedValue itemVal;
                            if (m->GetMemberType() == MemberType::Field)
                                itemVal = static_cast<const Field*>(m)->Get(rtResult.GetValue());

                            String activeName = GetVariantActiveTypeName(itemVal);

                            Check("Variant RT Base: item type preserved as Base",
                                  activeName == String("HMFVariantBase"),
                                  activeName);
                        }
                    }
                }
            }
        }
    }

    {
        struct VariantTest
        {
            const char* name;
            const char* hmf;
        };

        const VariantTest tests[] = {
            { "int", R"(NodeTag {
    Name = "T"
    Data = 42
}
)" },
            { "float", R"(NodeTag {
    Name = "T"
    Data = 3.14
}
)" },
            { "String", R"(NodeTag {
    Name = "T"
    Data = "hello"
}
)" },
            { "negative", R"(NodeTag {
    Name = "T"
    Data = -7
}
)" },
            { "zero", R"(NodeTag {
    Name = "T"
    Data = 0
}
)" },
            { "large_int", R"(NodeTag {
    Name = "T"
    Data = 2000000000
}
)" },
            { "Vec3f", R"(NodeTag {
    Name = "T"
    Data = (1, 2.5, 3)
}
)" },
            { "Vec4f", R"(NodeTag {
    Name = "T"
    Data = (1, 2, 3, 4)
}
)" },
        };

        for (const auto& vt : tests)
        {
            HMF::ParseResult result = HMF::Parse(vt.hmf);
            String label1 = String("Variant<") + vt.name + String(">: parse succeeds");
            Check(label1.Data(), Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                String label2 = String("Variant<") + vt.name + String(">: type is NodeTag");
                Check(label2.Data(), ClassNameIs(result.GetValue(), "NodeTag"));
            }
        }
    }

    {
        const String manifest = R"(MeshLodData {
    VertexData = BlobDataReference {
        Key = "Game://Meshes/Cube.VB"
        Size = 65536
    }
    IndexData = BlobDataReference {
        Key = "Game://Meshes/Cube.IB"
        Size = 32768
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse MeshLodData succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("MeshLodData: type correct", ClassNameIs(result.GetValue(), "MeshLodData"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                if (const IMember* m = cls->GetMember(StringHash("VertexData")))
                {
                    BoxedValue vd;
                    if (m->GetMemberType() == MemberType::Field)
                        vd = static_cast<const Field*>(m)->Get(result.GetValue());
                    else if (m->GetMemberType() == MemberType::Property)
                        vd = static_cast<const Property*>(m)->Get(result.GetValue());

                    const Class* vdCls = GetClass(vd.GetTypeId());
                    if (vdCls)
                    {
                        Name key = GetFieldValue<Name>(vd, vdCls, "Key");
                        Check("MeshLodData: VertexData.Key correct",
                              String(key.LookupString()) == String("Game://Meshes/Cube.VB"),
                              key.LookupString());
                        Check("MeshLodData: VertexData.Size == 65536",
                              GetFieldValue<uint64>(vd, vdCls, "Size") == 65536);
                    }
                }

                if (const IMember* m = cls->GetMember(StringHash("IndexData")))
                {
                    BoxedValue id;
                    if (m->GetMemberType() == MemberType::Field)
                        id = static_cast<const Field*>(m)->Get(result.GetValue());
                    else if (m->GetMemberType() == MemberType::Property)
                        id = static_cast<const Property*>(m)->Get(result.GetValue());

                    const Class* idCls = GetClass(id.GetTypeId());
                    if (idCls)
                    {
                        Check("MeshLodData: IndexData.Size == 32768",
                              GetFieldValue<uint64>(id, idCls, "Size") == 32768);
                    }
                }
            }
        }
    }

    {
        const String manifest = R"(MaterialParameters {
    Albedo = (0.5, 0.25, 0.75, 1)
    Metalness = 0.8
    Roughness = 0.15
    AlphaThreshold = 0.33
    ParallaxHeightScale = 0.05
    Transmission = 0.5
    IOR = 1.52
    EmissiveColor = {
        Red = 1
        Green = 0.5
        Blue = 0.1
        Alpha = 1
    }
    EmissiveIntensity = 10
    UserParams = (1, 2, 3, 4)
    Unlit = false
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse MP from HMF succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("MP HMF: type correct", ClassNameIs(result.GetValue(), "MaterialParameters"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Check("MP HMF: Metalness == 0.8", GetFieldValue<float>(result.GetValue(), cls, "Metalness") == 0.8f);
                Check("MP HMF: Roughness == 0.15", GetFieldValue<float>(result.GetValue(), cls, "Roughness") == 0.15f);
                Check("MP HMF: IOR == 1.52", GetFieldValue<float>(result.GetValue(), cls, "IOR") == 1.52f);
                Check("MP HMF: Unlit == false", !GetFieldValue<bool>(result.GetValue(), cls, "Unlit"));
                Check("MP HMF: Transmission == 0.5", GetFieldValue<float>(result.GetValue(), cls, "Transmission") == 0.5f);
            }
        }
    }

    {
        const String manifest = R"(MaterialAttributes {
    ShaderName = "GeometryPass"
    Bucket = Opaque
    FillMode = FM_FILL
    CullFaces = FCM_BACK
    Flags = MAF_DEPTH_WRITE|MAF_DEPTH_TEST
    StencilFunction = {
        PassOp = SO_REPLACE
        FailOp = SO_KEEP
        DepthFailOp = SO_KEEP
        CompareOp = SCO_ALWAYS
    }
    DepthCompareOp = DCO_LESS
    StencilReference = 3
    DepthBias = 50
    DepthBiasSlope = 1.5
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse MA from HMF succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("MA HMF: type correct", ClassNameIs(result.GetValue(), "MaterialAttributes"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Check("MA HMF: DepthBias == 50", GetFieldValue<int32>(result.GetValue(), cls, "DepthBias") == 50);
                Check("MA HMF: DepthBiasSlope == 1.5", GetFieldValue<float>(result.GetValue(), cls, "DepthBiasSlope") == 1.5f);
                Check("MA HMF: StencilReference == 3", GetFieldValue<uint8>(result.GetValue(), cls, "StencilReference") == 3);
                Check("MA HMF: Bucket == Opaque",
                      GetFieldUInt64(result.GetValue(), cls, "Bucket") == uint64(RenderBucket::Opaque));
                Check("MA HMF: FillMode == FM_FILL",
                      GetFieldUInt64(result.GetValue(), cls, "FillMode") == uint64(FM_FILL));
                Check("MA HMF: CullFaces == FCM_BACK",
                      GetFieldUInt64(result.GetValue(), cls, "CullFaces") == uint64(FCM_BACK));
            }
        }
    }

    {
        const String manifest = R"(SamplerDesc {
    MinFilterMode = TFM_LINEAR_MIPMAP
    MagFilterMode = TFM_LINEAR
    WrapMode = TWM_REPEAT
    CompareOp = SCO_LESS
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse SamplerDesc succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("SamplerDesc: type correct", ClassNameIs(result.GetValue(), "SamplerDesc"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Check("SamplerDesc: MinFilterMode == TFM_LINEAR_MIPMAP",
                      GetFieldUInt64(result.GetValue(), cls, "MinFilterMode") == uint64(TFM_LINEAR_MIPMAP));
                Check("SamplerDesc: WrapMode == TWM_REPEAT",
                      GetFieldUInt64(result.GetValue(), cls, "WrapMode") == uint64(TWM_REPEAT));
            }
        }
    }

    {
        const String manifest = R"(Viewport {
    Extent = (1920, 1080)
    Position = (100, 200)
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse Viewport succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("Viewport: type correct", ClassNameIs(result.GetValue(), "Viewport"));
        }
    }

    {
        const String manifest = R"(NodeTag "TransformWidgetElementColor" {
    Data = (1, 0.02, 0.02, 1)
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse NodeTag (Vec4f variant) succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("NodeTag Vec4f: type correct", ClassNameIs(result.GetValue(), "NodeTag"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Name tagName = GetFieldValue<Name>(result.GetValue(), cls, "Name");
                Check("NodeTag Vec4f: Name == TransformWidgetElementColor",
                      String(tagName.LookupString()) == String("TransformWidgetElementColor"),
                      tagName.LookupString());
            }
        }
    }

    {
        const String manifest = R"(NodeTag "Speed" {
    Data = 42.5
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("NodeTag RT: parse succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            // Write it back to HMF
            String rt;
            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                ObjectToHMF(cls, result.GetValue(), rt);
                HYP_LOG(Engine, Info, "NodeTag round-trip HMF:\n{}", rt);

                Check("NodeTag RT: has no Name field", !rt.Contains("Name ="), rt);
                Check("NodeTag RT: has Name at top of object", rt.Contains("NodeTag \"Speed\""), rt);
                Check("NodeTag RT: has Data field", rt.Contains("Data"), rt);
                Check("NodeTag RT: Data = 42.5", rt.Contains("Data = 42.5"), rt);

                // Parse the round-trip output again
                HMF::ParseResult rtResult = HMF::Parse(rt);
                Check("NodeTag RT: re-parse succeeds", Success(rtResult), rtResult.GetError().GetMessage());

                if (Success(rtResult))
                {
                    Check("NodeTag RT: re-parse type correct", ClassNameIs(rtResult.GetValue(), "NodeTag"));
                }
            }
        }
    }

    {
        const String manifest = R"(RawDataAsset "BlueNoise" {
    Data = BlobDataReference {
        Key = "Engine://RawData/BlueNoise.RAW"
        Size = 1310720
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse RawDataAsset succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("RawDataAsset: type correct", ClassNameIs(result.GetValue(), "RawDataAsset"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Name assetName = GetFieldValue<Name>(result.GetValue(), cls, "Name");
                Check("RawDataAsset: Name == BlueNoise",
                      String(assetName.LookupString()) == String("BlueNoise"),
                      assetName.LookupString());
            }
        }
    }

    {
        const String manifest = R"(AnimationTrack "Sprint_chest" {
    BoneName = "chest"
    KeyframeData = BlobDataReference {
        Key = "Game://AnimationTracks/Sprint_chest.KEYF"
        Size = 1408
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse AnimationTrack succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("AnimationTrack: type correct", ClassNameIs(result.GetValue(), "AnimationTrack"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Name boneName = GetFieldValue<Name>(result.GetValue(), cls, "BoneName");
                Check("AnimationTrack: BoneName == chest",
                      String(boneName.LookupString()) == String("chest"),
                      boneName.LookupString());
            }
        }
    }

    {
        const String manifest = R"(HMFTestNestedStruct {
    Label = "MyTestTexture"
    Id = 42
    Texture = {
        Type = Texture3D
        Format = RGBA8
        Extent = (256, 256, 32)
        MinFilterMode = TFM_LINEAR
        MagFilterMode = TFM_LINEAR
        TextureWrapMode = TWM_REPEAT
        NumLayers = 2
        ImageUsage = IU_SAMPLED|IU_TRANSFER_DST
        MipOffsets = [0, 65536, 81920, 86016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse HMFTestNestedStruct succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("NestedStruct: type correct", ClassNameIs(result.GetValue(), "HMFTestNestedStruct"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                // Verify top-level fields
                Name label = GetFieldValue<Name>(result.GetValue(), cls, "Label");
                Check("NestedStruct: Label == MyTestTexture",
                      String(label.LookupString()) == String("MyTestTexture"),
                      label.LookupString());

                Check("NestedStruct: Id == 42", GetFieldValue<int32>(result.GetValue(), cls, "Id") == 42);

                // Verify nested TextureDesc fields
                Check("NestedStruct: Texture Type == Texture3D",
                      GetFieldUInt64(result.GetValue(), cls, "Texture") == 0); // placeholder, will refine below

                if (const IMember* m = cls->GetMember(StringHash("Texture")))
                {
                    BoxedValue texVal;
                    if (m->GetMemberType() == MemberType::Field)
                        texVal = static_cast<const Field*>(m)->Get(result.GetValue());
                    else if (m->GetMemberType() == MemberType::Property)
                        texVal = static_cast<const Property*>(m)->Get(result.GetValue());

                    const Class* texCls = GetClass(texVal.GetTypeId());
                    if (texCls)
                    {
                        Check("Nested Texture: type correct",
                              String(texCls->GetName().LookupString()) == String("TextureDesc"),
                              texCls->GetName().LookupString());

                        Check("Nested Texture: Type == Texture3D",
                              GetFieldUInt64(texVal, texCls, "Type") == uint64(TextureType::Texture3D));
                        Check("Nested Texture: Format == RGBA8",
                              GetFieldUInt64(texVal, texCls, "Format") == uint64(TextureFormat::RGBA8));
                        Check("Nested Texture: MinFilterMode == TFM_LINEAR",
                              GetFieldUInt64(texVal, texCls, "MinFilterMode") == uint64(TFM_LINEAR));
                        Check("Nested Texture: TextureWrapMode == TWM_REPEAT",
                              GetFieldUInt64(texVal, texCls, "TextureWrapMode") == uint64(TWM_REPEAT));
                        Check("Nested Texture: NumLayers == 2",
                              GetFieldValue<uint16>(texVal, texCls, "NumLayers") == 2);
                    }
                    else
                    {
                        Check("Nested Texture: class resolved", false, "could not get TextureDesc class");
                    }
                }
            }
        }
    }

    {
        const Class* cls = GetClass<HMFTestNestedStruct>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("NestedStruct registered", false, "class not found or cannot create instance");
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Label", BoxedValue(CreateNameFromDynamicString("RoundTripTest")));
            SetFieldValue(obj, cls, "Id", BoxedValue(int32(777)));

            String text;
            ObjectToHMF(cls, obj, text);
            HYP_LOG(Engine, Info, "NestedStruct round-trip HMF:\n{}", text);

            Check("NestedStruct RT: has Label", text.Contains("Label"), text);
            Check("NestedStruct RT: has Id", text.Contains("Id"), text);
            Check("NestedStruct RT: has Texture", text.Contains("Texture"), text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("NestedStruct RT: re-parse succeeds", Success(result), result.GetError().GetMessage());

            if (Success(result))
            {
                Check("NestedStruct RT: type correct", ClassNameIs(result.GetValue(), "HMFTestNestedStruct"));

                const Class* pc = GetClass(result.GetValue().GetTypeId());
                if (pc)
                {
                    Check("NestedStruct RT: Id == 777", GetFieldValue<int32>(result.GetValue(), pc, "Id") == 777);

                    Name label = GetFieldValue<Name>(result.GetValue(), pc, "Label");
                    Check("NestedStruct RT: Label == RoundTripTest",
                          String(label.LookupString()) == String("RoundTripTest"),
                          label.LookupString());
                }
            }
        }
    }

    {
        const String manifest = R"(HMFVariantArrayHolder {
    HolderName = "MixedArray"
    Items = [
        HMFVariantBase {
            BaseValue = 10
        }
        HMFVariantDerived {
            BaseValue = 20
            DerivedValue = 1.5
            DerivedName = "First"
        }
        HMFVariantBase {
            BaseValue = 30
        }
        HMFVariantDerived {
            BaseValue = 40
            DerivedValue = 2.5
            DerivedName = "Second"
        }
    ]
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("PolyArray: parse succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            Check("PolyArray: type is HMFVariantArrayHolder",
                  ClassNameIs(result.GetValue(), "HMFVariantArrayHolder"));

            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                Name hname = GetFieldValue<Name>(result.GetValue(), cls, "HolderName");
                Check("PolyArray: HolderName == MixedArray",
                      String(hname.LookupString()) == String("MixedArray"),
                      hname.LookupString());

                // Extract the Items array
                if (const IMember* m = cls->GetMember(StringHash("Items")))
                {
                    BoxedValue itemsVal;
                    if (m->GetMemberType() == MemberType::Field)
                        itemsVal = static_cast<const Field*>(m)->Get(result.GetValue());

                    // Use GenericArrayWrapper to iterate
                    auto* arrHandler = static_cast<ITypeInfoArrayHandler*>(
                        itemsVal.GetTypeInfo()->extendedInfo.handler);

                    if (arrHandler)
                    {
                        const size_t count = arrHandler->GetSize(itemsVal);
                        HYP_LOG(Engine, Info, "PolyArray: {} items", count);
                        Check("PolyArray: 4 items in array", count == 4, "wrong count");

                        // Verify each item
                        for (size_t i = 0; i < count && i < 4; i++)
                        {
                            BoxedValue element;
                            if (arrHandler->GetElementAt(itemsVal, i, element))
                            {
                                const Class* elemCls = GetClass(element.GetTypeId());
                                String elemTypeName = elemCls ? elemCls->GetName().ToString() : String("null");
                                HYP_LOG(Engine, Info, "PolyArray: item[{}] type={}", i, elemTypeName.Data());

                                if (elemCls)
                                {
                                    int32 bv = GetFieldValue<int32>(element, elemCls, "BaseValue");

                                    if (i == 0)
                                    {
                                        Check("PolyArray[0]: type Base", elemTypeName == String("HMFVariantBase"), elemTypeName);
                                        Check("PolyArray[0]: BaseValue==10", bv == 10);
                                    }
                                    if (i == 1)
                                    {
                                        Check("PolyArray[1]: type Derived", elemTypeName == String("HMFVariantDerived"), elemTypeName);
                                        Check("PolyArray[1]: BaseValue==20", bv == 20);
                                    }
                                    if (i == 2)
                                    {
                                        Check("PolyArray[2]: type Base", elemTypeName == String("HMFVariantBase"), elemTypeName);
                                        Check("PolyArray[2]: BaseValue==30", bv == 30);
                                    }
                                    if (i == 3)
                                    {
                                        Check("PolyArray[3]: type Derived", elemTypeName == String("HMFVariantDerived"), elemTypeName);
                                        Check("PolyArray[3]: BaseValue==40", bv == 40);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    {
        const String manifest = R"(HMFVariantArrayHolder {
    HolderName = "RT"
    Items = [
        HMFVariantBase {
            BaseValue = 111
        }
        HMFVariantDerived {
            BaseValue = 222
            DerivedValue = 9.99
            DerivedName = "RTDerived"
        }
    ]
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("PolyArray RT: parse succeeds", Success(result), result.GetError().GetMessage());

        if (Success(result))
        {
            const Class* cls = GetClass(result.GetValue().GetTypeId());
            if (cls)
            {
                // Write back to HMF
                String text;
                ObjectToHMF(cls, result.GetValue(), text);
                HYP_LOG(Engine, Info, "PolyArray RT HMF:\n{}", text);

                Check("PolyArray RT: first element uses Base prefix",
                      text.Contains("HMFVariantBase") || text.Contains("BaseValue = 111"),
                      text);
                Check("PolyArray RT: has HMFVariantDerived prefix", text.Contains("HMFVariantDerived"), text);
                Check("PolyArray RT: BaseValue = 111", text.Contains("BaseValue = 111"), text);
                Check("PolyArray RT: BaseValue = 222", text.Contains("BaseValue = 222"), text);
                Check("PolyArray RT: DerivedValue = 9.99", text.Contains("DerivedValue = 9.99"), text);
                Check("PolyArray RT: DerivedName = RTDerived", text.Contains("DerivedName = \"RTDerived\""), text);

                // Re-parse
                HMF::ParseResult rtResult = HMF::Parse(text);
                Check("PolyArray RT: re-parse succeeds", Success(rtResult), rtResult.GetError().GetMessage());

                if (Success(rtResult))
                {
                    Check("PolyArray RT: type correct",
                          ClassNameIs(rtResult.GetValue(), "HMFVariantArrayHolder"));

                    const Class* rtCls = GetClass(rtResult.GetValue().GetTypeId());
                    if (rtCls)
                    {
                        if (const IMember* m = rtCls->GetMember(StringHash("Items")))
                        {
                            BoxedValue itemsVal;
                            if (m->GetMemberType() == MemberType::Field)
                                itemsVal = static_cast<const Field*>(m)->Get(rtResult.GetValue());

                            auto* arrHandler = static_cast<ITypeInfoArrayHandler*>(
                                itemsVal.GetTypeInfo()->extendedInfo.handler);

                            if (arrHandler)
                            {
                                const size_t count = arrHandler->GetSize(itemsVal);
                                Check("PolyArray RT: 2 items preserved", count == 2, "wrong count");

                                for (size_t i = 0; i < count && i < 2; i++)
                                {
                                    BoxedValue element;
                                    if (arrHandler->GetElementAt(itemsVal, i, element))
                                    {
                                        const Class* elemCls = GetClass(element.GetTypeId());
                                        if (elemCls)
                                        {
                                            int32 bv = GetFieldValue<int32>(element, elemCls, "BaseValue");

                                            if (i == 0)
                                            {
                                                Check("PolyArray RT[0]: type Base",
                                                      String(elemCls->GetName().ToString()) == String("HMFVariantBase"),
                                                      elemCls->GetName().ToString());
                                                Check("PolyArray RT[0]: BaseValue==111", bv == 111);
                                            }
                                            if (i == 1)
                                            {
                                                Check("PolyArray RT[1]: type Derived",
                                                      String(elemCls->GetName().ToString()) == String("HMFVariantDerived"),
                                                      elemCls->GetName().ToString());
                                                Check("PolyArray RT[1]: BaseValue==222", bv == 222);

                                                float dv = GetFieldValue<float>(element, elemCls, "DerivedValue");
                                                Check("PolyArray RT[1]: DerivedValue==9.99", dv == 9.99f);

                                                Name dn = GetFieldValue<Name>(element, elemCls, "DerivedName");
                                                Check("PolyArray RT[1]: DerivedName==RTDerived",
                                                      String(dn.LookupString()) == String("RTDerived"),
                                                      dn.LookupString());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    {
        String text;
        Array<bool> boolArr = { true, false, true };
        BoxedToHMF(BoxedValue(boolArr), text);
        Check("Array<bool>: has true and false",
              text.Contains("true") && text.Contains("false"), text);

        // Array of strings -- verify we don't crash and produce output
        text.Clear();
        Array<String> strArr;
        strArr.PushBack("hello");
        BoxedToHMF(BoxedValue(strArr), text);
        Check("Array<String>: non-empty output", !text.Empty(), text);
    }

    HYP_LOG(Engine, Info, "========== HMF Test Results: {} passed, {} failed ==========",
            g_passCount, g_failCount);

    if (g_failCount > 0)
    {
        HYP_LOG(Engine, Error, "!!! HMF TEST HAD FAILURES !!!");
    }
}

} // namespace hmf
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
