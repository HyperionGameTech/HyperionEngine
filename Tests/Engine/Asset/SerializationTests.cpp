/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Asset/SerializationUtils.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>
#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Transform.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <limits>

namespace Hyperion {
namespace tests {
namespace serialization {

namespace {

// Writes the value as HMF, then reads the text back as the same type
template <class T>
bool RoundTripHMF(const T& value, T& outValue, String& outText)
{
    const TypeInfo& typeInfo = TypeOf<T>();

    if (BoxedToHMF(BoxedValue(value), outText, &typeInfo).HasError())
    {
        return false;
    }

    HMF::ParseResult parsed = HMF::ParseValue(outText, typeInfo);

    if (parsed.HasError() || !parsed.GetValue().Is<T>())
    {
        return false;
    }

    outValue = parsed.GetValue().Get<T>();

    return true;
}

template <class T>
void CheckRoundTripHMF(EngineTestContext& context, const char* label, const T& value)
{
    T parsed {};
    String text;

    if (!context.Check(label, RoundTripHMF(value, parsed, text), HYP_FORMAT("couldn't round trip '{}'", text)))
    {
        return;
    }

    context.Check(label, parsed == value, HYP_FORMAT("text was '{}'", text));
}

Handle<Node> AsNode(const BoxedValue& value)
{
    if (value.Is<Handle<Node>>())
    {
        return value.Get<Handle<Node>>();
    }

    if (value.Is<Handle<ObjectBase>>())
    {
        return TryCast<Node>(value.Get<Handle<ObjectBase>>());
    }

    return Handle<Node>();
}

HYP_ENGINE_TEST(Serialization, IntegersRoundTripHMF)
{
    CheckRoundTripHMF<int8>(context, "int8 min", -128);
    CheckRoundTripHMF<int16>(context, "int16 min", -32768);
    CheckRoundTripHMF<int32>(context, "int32 min", std::numeric_limits<int32>::min());
    CheckRoundTripHMF<int32>(context, "int32 max", std::numeric_limits<int32>::max());
    CheckRoundTripHMF<int64>(context, "int64 negative", int64(-9999999999LL));
    CheckRoundTripHMF<uint8>(context, "uint8 max", 255);
    CheckRoundTripHMF<uint16>(context, "uint16 max", 65535);
    CheckRoundTripHMF<uint32>(context, "uint32 max", 0xFFFFFFFFu);
    CheckRoundTripHMF<uint64>(context, "uint64 max", 0xFFFFFFFFFFFFFFFFULL);
    CheckRoundTripHMF<int32>(context, "zero", 0);
}

HYP_ENGINE_TEST(Serialization, FloatsRoundTripHMFExactly)
{
    CheckRoundTripHMF<float>(context, "float 0.1", 0.1f);
    CheckRoundTripHMF<float>(context, "float negative", -123.456f);
    CheckRoundTripHMF<float>(context, "float tiny", 1.0e-7f);
    CheckRoundTripHMF<float>(context, "float large", 3.0e+30f);
    CheckRoundTripHMF<float>(context, "float zero", 0.0f);
    CheckRoundTripHMF<double>(context, "double pi", 3.14159265358979);
    CheckRoundTripHMF<double>(context, "double 0.1", 0.1);
}

HYP_ENGINE_TEST(Serialization, BoolsRoundTripHMF)
{
    CheckRoundTripHMF<bool>(context, "true", true);
    CheckRoundTripHMF<bool>(context, "false", false);
}

HYP_ENGINE_TEST(Serialization, StringsRoundTripHMF)
{
    CheckRoundTripHMF<String>(context, "empty", String(""));
    CheckRoundTripHMF<String>(context, "plain", String("hello world"));
    CheckRoundTripHMF<String>(context, "quotes", String("she said \"hi\""));
    CheckRoundTripHMF<String>(context, "backslashes", String("C:\\path\\to\\file"));
    CheckRoundTripHMF<String>(context, "newline and tab", String("line\nbreak\ttab"));
    CheckRoundTripHMF<String>(context, "looks like a number", String("42"));
    CheckRoundTripHMF<String>(context, "looks like a keyword", String("true"));
}

HYP_ENGINE_TEST(Serialization, VectorsRoundTripHMF)
{
    CheckRoundTripHMF<Vec3f>(context, "Vec3f", Vec3f(1.5f, -2.25f, 3.125f));
    CheckRoundTripHMF<Vec3f>(context, "Vec3f zero", Vec3f::Zero());
    CheckRoundTripHMF<Vec4f>(context, "Vec4f", Vec4f(0.1f, 0.2f, 0.3f, 0.4f));
}

HYP_ENGINE_TEST(Serialization, ArraysRoundTripHMF)
{
    CheckRoundTripHMF<Array<int32>>(context, "Array<int32>", Array<int32> { 1, -2, 3, 4 });
    CheckRoundTripHMF<Array<int32>>(context, "empty Array<int32>", Array<int32> {});
    CheckRoundTripHMF<Array<String>>(context, "Array<String>", Array<String> { String("a"), String("b \"quoted\""), String("") });
    CheckRoundTripHMF<Array<Vec3f>>(context, "Array<Vec3f>", Array<Vec3f> { Vec3f(1.0f, 2.0f, 3.0f), Vec3f(-1.0f, 0.5f, 0.25f) });
}

HYP_ENGINE_TEST(Serialization, EnumFlagsRoundTripHMF)
{
    CheckRoundTripHMF<EnumFlags<NodeFlags>>(context, "none", EnumFlags<NodeFlags>(NodeFlags::None));
    CheckRoundTripHMF<EnumFlags<NodeFlags>>(context, "single flag", EnumFlags<NodeFlags>(NodeFlags::IgnoreParentTranslation));
    CheckRoundTripHMF<EnumFlags<NodeFlags>>(context, "two flags", EnumFlags<NodeFlags>(NodeFlags::IgnoreParentTranslation | NodeFlags::IgnoreParentScale));
}

HYP_ENGINE_TEST(Serialization, TransformRoundTripsHMF)
{
    Transform transform(Vec3f(1.0f, 2.0f, 3.0f), Vec3f(2.0f, 2.0f, 2.0f), Quat4f::AxisAngles(Vec3f(0.0f, 1.0f, 0.0f), 0.5f));

    Transform parsed;
    String text;

    HYP_TEST_REQUIRE(RoundTripHMF(transform, parsed, text));

    HYP_TEST_CHECK(parsed.GetTranslation() == transform.GetTranslation());
    HYP_TEST_CHECK(parsed.GetScale() == transform.GetScale());
    HYP_TEST_CHECK(parsed.GetRotation() == transform.GetRotation());
}

HYP_ENGINE_TEST(Serialization, MalformedHMFReportsErrors)
{
    const char* const malformed[] = {
        "Node {",
        "Node { Name = }",
        "Node { Name = \"unterminated }",
        "NotARegisteredClass { Value = 1 }",
        "{ }}",
        "Node { Name = \"A\" Name = }",
    };

    for (const char* source : malformed)
    {
        HMF::ErrorList errors;
        HMF::ParseResult result = HMF::Parse(String(source), &errors);

        context.Check(source, result.HasError() || errors.HasFatalErrors(), "neither failed nor reported an error");
    }
}

HYP_ENGINE_TEST(Serialization, TrailingTokensAfterValueAreRejected)
{
    HYP_TEST_CHECK(HMF::ParseValue("42", TypeOf<int32>()).HasValue());
    HYP_TEST_CHECK(HMF::ParseValue("42 43", TypeOf<int32>()).HasError());
    HYP_TEST_CHECK(HMF::ParseValue("\"text\"", TypeOf<int32>()).HasError());
    HYP_TEST_CHECK(HMF::ParseValue("", TypeOf<int32>()).HasError());
}

HYP_ENGINE_TEST(Serialization, NodeRoundTripsHMF)
{
    Handle<Scene> scene = MakeHandle<Scene>(NAME("SerializationNodeRoundTripScene"), ThreadId::Current(), SceneFlags::NONE);
    InitObject(scene);

    Handle<Node> node = MakeHandle<Node>();
    node->SetName(NAME("SerializedNode"));
    node->SetLocalTransform(Transform(Vec3f(1.0f, 2.0f, 3.0f), Vec3f(2.0f), Quat4f::Identity()));
    node->AddDataEntry(NodeData(NAME("Health"), 42));

    scene->GetRoot()->AddChild(node);

    String text;
    HYP_TEST_REQUIRE(!ObjectToHMF(Node::StaticClass(), BoxedValue(node), text).HasError());

    HYP_TEST_CHECK(text.Contains("SerializedNode"));
    HYP_TEST_CHECK(text.Contains("Health"));

    HMF::ParseResult parsed = HMF::Parse(text);

    HYP_TEST_REQUIRE(parsed.HasValue());

    Handle<Node> loaded = AsNode(parsed.GetValue());

    HYP_TEST_REQUIRE(loaded.IsValid());

    HYP_TEST_CHECK(loaded != node);
    HYP_TEST_CHECK(loaded->GetName() == NAME("SerializedNode"));
    HYP_TEST_CHECK(loaded->GetUUID() == node->GetUUID());
    HYP_TEST_CHECK(loaded->GetLocalTransform() == node->GetLocalTransform());
    HYP_TEST_CHECK(loaded->HasDataEntry("Health"_sh));
    HYP_TEST_CHECK(loaded->GetParent() == nullptr);
}

HYP_ENGINE_TEST(Serialization, NodeLoadsLegacyNodeTags)
{
    Handle<Node> node = MakeHandle<Node>();
    node->SetName(NAME("LegacyNode"));
    node->AddDataEntry(NodeData(NAME("Health"), 42));

    String text;
    HYP_TEST_REQUIRE(!ObjectToHMF(Node::StaticClass(), BoxedValue(node), text).HasError());

    const String legacy = text.ReplaceAll("DataEntries", "NodeTags").ReplaceAll("NodeData", "NodeTag");

    HYP_TEST_REQUIRE_MSG(legacy.Contains("NodeTags") && legacy.Contains("NodeTag "), "the legacy names weren't written:\n{}", legacy);

    HMF::ParseResult parsed = HMF::Parse(legacy);

    HYP_TEST_REQUIRE(parsed.HasValue());

    Handle<Node> loaded = AsNode(parsed.GetValue());

    HYP_TEST_REQUIRE(loaded.IsValid());

    HYP_TEST_CHECK(loaded->HasDataEntry("Health"_sh));
}

HYP_ENGINE_TEST(Serialization, NodeTreeKeepsChildrenOrderAndParents)
{
    Handle<Scene> scene = MakeHandle<Scene>(NAME("SerializationTreeScene"), ThreadId::Current(), SceneFlags::NONE);
    InitObject(scene);

    Handle<Node> parent = MakeHandle<Node>();
    parent->SetName(NAME("Parent"));

    const char* const childNames[] = { "ChildC", "ChildA", "ChildB" };

    for (const char* childName : childNames)
    {
        Handle<Node> child = MakeHandle<Node>();
        child->SetName(CreateNameFromDynamicString(childName));
        child->SetLocalTranslation(Vec3f(float(parent->NumChildren()), 0.0f, 0.0f));

        parent->AddChild(child);
    }

    Handle<Node> grandchild = MakeHandle<Node>();
    grandchild->SetName(NAME("Grandchild"));
    parent->GetChild(1)->AddChild(grandchild);

    scene->GetRoot()->AddChild(parent);

    String text;
    HYP_TEST_REQUIRE(!ObjectToHMF(Node::StaticClass(), BoxedValue(parent), text).HasError());

    HMF::ParseResult parsed = HMF::Parse(text);

    HYP_TEST_REQUIRE(parsed.HasValue());

    Handle<Node> loaded = AsNode(parsed.GetValue());

    HYP_TEST_REQUIRE(loaded.IsValid());
    HYP_TEST_REQUIRE(loaded->NumChildren() == 3);

    for (uint32 index = 0; index < 3; index++)
    {
        Handle<Node> loadedChild = loaded->GetChild(index);

        HYP_TEST_REQUIRE(loadedChild.IsValid());

        context.Check("child order", loadedChild->GetName() == CreateNameFromDynamicString(childNames[index]), HYP_FORMAT("child {}", index));
        context.Check("child parent pointer", loadedChild->GetParent() == loaded.Get(), HYP_FORMAT("child {}", index));
        context.Check("child local translation", loadedChild->GetLocalTranslation() == Vec3f(float(index), 0.0f, 0.0f), HYP_FORMAT("child {}", index));
    }

    HYP_TEST_REQUIRE(loaded->GetChild(1)->NumChildren() == 1);
    HYP_TEST_CHECK(loaded->GetChild(1)->GetChild(0)->GetName() == NAME("Grandchild"));
}

HYP_ENGINE_TEST(Serialization, TransientNodeMembersAreNotWritten)
{
    Handle<Scene> scene = MakeHandle<Scene>(NAME("SerializationTransientScene"), ThreadId::Current(), SceneFlags::NONE);
    InitObject(scene);

    Handle<Node> node = MakeHandle<Node>();
    node->SetName(NAME("Transient"));
    scene->GetRoot()->AddChild(node);

    String text;
    HYP_TEST_REQUIRE(!ObjectToHMF(Node::StaticClass(), BoxedValue(node), text).HasError());

    HYP_TEST_CHECK(!text.Contains("Parent"));
    HYP_TEST_CHECK(!text.Contains("WorldMatrix"));
    HYP_TEST_CHECK(!text.Contains("WorldTranslation"));
}

HYP_ENGINE_TEST(Serialization, NodeRoundTripsJSON)
{
    Handle<Node> node = MakeHandle<Node>();
    node->SetName(NAME("JsonNode"));
    node->SetLocalTranslation(Vec3f(4.0f, 5.0f, 6.0f));

    JSON::Object json;
    HYP_TEST_REQUIRE(!ObjectToJSON(Node::StaticClass(), BoxedValue(node), json).HasError());

    BoxedValue loadedValue;
    HYP_TEST_REQUIRE(Node::StaticClass()->CreateInstance(loadedValue));
    HYP_TEST_REQUIRE(!ObjectFromJSON(json, Node::StaticClass(), loadedValue).HasError());

    Handle<Node> loaded = AsNode(loadedValue);

    HYP_TEST_REQUIRE(loaded.IsValid());

    HYP_TEST_CHECK(loaded->GetName() == NAME("JsonNode"));
    HYP_TEST_CHECK(loaded->GetLocalTranslation() == Vec3f(4.0f, 5.0f, 6.0f));
}

HYP_ENGINE_TEST(Serialization, PrimitivesRoundTripJSON)
{
    const TypeInfo& intTypeInfo = TypeOf<int32>();
    const TypeInfo& stringTypeInfo = TypeOf<String>();
    const TypeInfo& vecTypeInfo = TypeOf<Vec3f>();

    JSON::Value json;
    BoxedValue boxed;

    HYP_TEST_REQUIRE(!BoxedToJSON(BoxedValue(int32(-17)), json).HasError());
    HYP_TEST_REQUIRE(!BoxedFromJSON(json, intTypeInfo, boxed).HasError());
    HYP_TEST_CHECK(boxed.Is<int32>() && boxed.Get<int32>() == -17);

    HYP_TEST_REQUIRE(!BoxedToJSON(BoxedValue(String("text \"with\" quotes")), json).HasError());
    HYP_TEST_REQUIRE(!BoxedFromJSON(json, stringTypeInfo, boxed).HasError());
    HYP_TEST_CHECK(boxed.Is<String>() && boxed.Get<String>() == "text \"with\" quotes");

    HYP_TEST_REQUIRE(!BoxedToJSON(BoxedValue(Vec3f(1.0f, 2.0f, 3.0f)), json).HasError());
    HYP_TEST_REQUIRE(!BoxedFromJSON(json, vecTypeInfo, boxed).HasError());
    HYP_TEST_CHECK(boxed.Is<Vec3f>() && boxed.Get<Vec3f>() == Vec3f(1.0f, 2.0f, 3.0f));
}

} // namespace

} // namespace serialization
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
