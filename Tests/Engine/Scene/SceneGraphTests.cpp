/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Components/CharacterModelComponent.hpp>
#include <Scene/Components/InstanceClusterComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Transform.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

constexpr float g_tolerance = 1e-4f;

bool Near(const Vec3f& a, const Vec3f& b)
{
    return MathUtil::Abs(a.x - b.x) <= g_tolerance
        && MathUtil::Abs(a.y - b.y) <= g_tolerance
        && MathUtil::Abs(a.z - b.z) <= g_tolerance;
}

Handle<Scene> CreateTestScene(const char* name)
{
    Handle<Scene> scene = MakeHandle<Scene>(CreateNameFromDynamicString(name), ThreadId::Current(), SceneFlags::NONE);
    InitObject(scene);

    return scene;
}

Handle<Node> CreateNode(const char* name, const Vec3f& translation = Vec3f::Zero())
{
    Handle<Node> node = MakeHandle<Node>();
    node->SetName(CreateNameFromDynamicString(name));
    node->SetLocalTranslation(translation);

    return node;
}

HYP_ENGINE_TEST(SceneGraph, AddChildSetsParentAndScene)
{
    Handle<Scene> scene = CreateTestScene("AddChildSetsParentAndScene");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> child = CreateNode("Child");

    HYP_TEST_CHECK(child->GetParent() == nullptr);
    HYP_TEST_CHECK(child->IsRoot());

    root->AddChild(child);

    HYP_TEST_CHECK(child->GetParent() == root.Get());
    HYP_TEST_CHECK(child->GetScene() == scene.Get());
    HYP_TEST_CHECK(!child->IsRoot());
    HYP_TEST_CHECK(root->NumChildren() == 1);
    HYP_TEST_CHECK(root->GetChild(0) == child);
    HYP_TEST_CHECK(root->GetChild(1) == Handle<Node>());
    HYP_TEST_CHECK(child->IsOrHasParent(root.Get()));
    HYP_TEST_CHECK(!root->IsOrHasParent(child.Get()));
}

HYP_ENGINE_TEST(SceneGraph, AddChildToSameParentIsNoOp)
{
    Handle<Scene> scene = CreateTestScene("AddChildToSameParentIsNoOp");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> child = CreateNode("Child");

    root->AddChild(child);
    root->AddChild(child);

    HYP_TEST_CHECK(root->NumChildren() == 1);
}

HYP_ENGINE_TEST(SceneGraph, AddChildReparents)
{
    Handle<Scene> scene = CreateTestScene("AddChildReparents");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> firstParent = CreateNode("FirstParent");
    Handle<Node> secondParent = CreateNode("SecondParent");
    Handle<Node> child = CreateNode("Child");

    root->AddChild(firstParent);
    root->AddChild(secondParent);

    firstParent->AddChild(child);

    HYP_TEST_CHECK(firstParent->NumChildren() == 1);

    secondParent->AddChild(child);

    HYP_TEST_CHECK(firstParent->NumChildren() == 0);
    HYP_TEST_CHECK(secondParent->NumChildren() == 1);
    HYP_TEST_CHECK(child->GetParent() == secondParent.Get());
}

HYP_ENGINE_TEST(SceneGraph, InsertChildPlacesAtIndex)
{
    Handle<Scene> scene = CreateTestScene("InsertChildPlacesAtIndex");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> first = CreateNode("First");
    Handle<Node> second = CreateNode("Second");
    Handle<Node> inserted = CreateNode("Inserted");
    Handle<Node> pastTheEnd = CreateNode("PastTheEnd");

    root->AddChild(first);
    root->AddChild(second);
    root->InsertChild(inserted, 1);
    root->InsertChild(pastTheEnd, 100);

    HYP_TEST_REQUIRE(root->NumChildren() == 4);

    HYP_TEST_CHECK(root->GetChild(0) == first);
    HYP_TEST_CHECK(root->GetChild(1) == inserted);
    HYP_TEST_CHECK(root->GetChild(2) == second);
    HYP_TEST_CHECK(root->GetChild(3) == pastTheEnd);

    HYP_TEST_CHECK(first->FindSelfIndex() == 0);
    HYP_TEST_CHECK(inserted->FindSelfIndex() == 1);
    HYP_TEST_CHECK(second->FindSelfIndex() == 2);
    HYP_TEST_CHECK(pastTheEnd->FindSelfIndex() == 3);
}

HYP_ENGINE_TEST(SceneGraph, RemoveClearsParentAndScene)
{
    Handle<Scene> scene = CreateTestScene("RemoveClearsParentAndScene");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> kept = CreateNode("Kept");
    Handle<Node> removed = CreateNode("Removed");

    root->AddChild(kept);
    root->AddChild(removed);

    HYP_TEST_CHECK(removed->Remove(/* moveToDetached */ false));

    HYP_TEST_CHECK(removed->GetParent() == nullptr);
    HYP_TEST_CHECK(removed->GetScene() == nullptr);
    HYP_TEST_CHECK(root->NumChildren() == 1);
    HYP_TEST_CHECK(root->GetChild(0) == kept);
    HYP_TEST_CHECK(!root->RemoveChild(removed.Get()));
}

HYP_ENGINE_TEST(SceneGraph, RemoveAtAndRemoveAllChildren)
{
    Handle<Scene> scene = CreateTestScene("RemoveAtAndRemoveAllChildren");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> nodes[3] = { CreateNode("A"), CreateNode("B"), CreateNode("C") };

    for (const Handle<Node>& node : nodes)
    {
        root->AddChild(node);
    }

    HYP_TEST_CHECK(root->RemoveAt(1, /* moveToDetached */ false));
    HYP_TEST_CHECK(root->NumChildren() == 2);
    HYP_TEST_CHECK(root->GetChild(0) == nodes[0]);
    HYP_TEST_CHECK(root->GetChild(1) == nodes[2]);
    HYP_TEST_CHECK(nodes[1]->GetParent() == nullptr);
    HYP_TEST_CHECK(!root->RemoveAt(5, /* moveToDetached */ false));

    root->RemoveAllChildren(/* moveToDetached */ false);

    HYP_TEST_CHECK(root->NumChildren() == 0);
    HYP_TEST_CHECK(nodes[0]->GetParent() == nullptr);
    HYP_TEST_CHECK(nodes[2]->GetParent() == nullptr);
}

HYP_ENGINE_TEST(SceneGraph, ReattachAfterRemove)
{
    Handle<Scene> scene = CreateTestScene("ReattachAfterRemove");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> child = CreateNode("Child");

    root->AddChild(child);
    child->Remove(/* moveToDetached */ false);
    root->AddChild(child);

    HYP_TEST_CHECK(child->GetParent() == root.Get());
    HYP_TEST_CHECK(child->GetScene() == scene.Get());
    HYP_TEST_CHECK(root->NumChildren() == 1);
}

HYP_ENGINE_TEST(SceneGraph, SceneIsSetOnDescendants)
{
    Handle<Scene> scene = CreateTestScene("SceneIsSetOnDescendants");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> parent = CreateNode("Parent");
    Handle<Node> child = CreateNode("Child");
    Handle<Node> grandchild = CreateNode("Grandchild");

    parent->AddChild(child);
    child->AddChild(grandchild);

    root->AddChild(parent);

    HYP_TEST_CHECK(parent->GetScene() == scene.Get());
    HYP_TEST_CHECK(child->GetScene() == scene.Get());
    HYP_TEST_CHECK(grandchild->GetScene() == scene.Get());

    parent->Remove(/* moveToDetached */ false);

    HYP_TEST_CHECK(parent->GetScene() == nullptr);
    HYP_TEST_CHECK(child->GetScene() == nullptr);
    HYP_TEST_CHECK(grandchild->GetScene() == nullptr);
}

HYP_ENGINE_TEST(SceneGraph, WorldTranslationFollowsParent)
{
    Handle<Scene> scene = CreateTestScene("WorldTranslationFollowsParent");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> parent = CreateNode("Parent", Vec3f(1.0f, 2.0f, 3.0f));
    Handle<Node> child = CreateNode("Child", Vec3f(4.0f, 5.0f, 6.0f));

    root->AddChild(parent);
    parent->AddChild(child);

    HYP_TEST_CHECK(Near(child->GetWorldTranslation(), Vec3f(5.0f, 7.0f, 9.0f)));

    parent->SetLocalTranslation(Vec3f(-1.0f, 0.0f, 0.0f));

    HYP_TEST_CHECK(Near(child->GetLocalTranslation(), Vec3f(4.0f, 5.0f, 6.0f)));
    HYP_TEST_CHECK(Near(child->GetWorldTranslation(), Vec3f(3.0f, 5.0f, 6.0f)));
}

HYP_ENGINE_TEST(SceneGraph, WorldTransformComposesScaleAndRotation)
{
    Handle<Scene> scene = CreateTestScene("WorldTransformComposesScaleAndRotation");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> parent = CreateNode("Parent", Vec3f(10.0f, 0.0f, 0.0f));
    parent->SetLocalScale(Vec3f(2.0f));

    Handle<Node> child = CreateNode("Child", Vec3f(1.0f, 0.0f, 0.0f));

    root->AddChild(parent);
    parent->AddChild(child);

    HYP_TEST_CHECK(Near(child->GetWorldTranslation(), Vec3f(12.0f, 0.0f, 0.0f)));
    HYP_TEST_CHECK(Near(child->GetWorldScale(), Vec3f(2.0f)));

    // a quarter turn about Y takes the child's local +X to world -Z
    parent->SetLocalScale(Vec3f(1.0f));
    parent->SetLocalRotation(Quat4f::AxisAngles(Vec3f(0.0f, 1.0f, 0.0f), MathUtil::pi<float> * 0.5f));

    HYP_TEST_CHECK(Near(child->GetWorldTranslation(), Vec3f(10.0f, 0.0f, -1.0f)));
}

HYP_ENGINE_TEST(SceneGraph, SetWorldTranslationCompensatesForParent)
{
    Handle<Scene> scene = CreateTestScene("SetWorldTranslationCompensatesForParent");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> parent = CreateNode("Parent", Vec3f(5.0f, 0.0f, 0.0f));
    Handle<Node> child = CreateNode("Child");

    root->AddChild(parent);
    parent->AddChild(child);

    child->SetWorldTranslation(Vec3f(8.0f, 1.0f, 0.0f));

    HYP_TEST_CHECK(Near(child->GetWorldTranslation(), Vec3f(8.0f, 1.0f, 0.0f)));
    HYP_TEST_CHECK(Near(child->GetLocalTranslation(), Vec3f(3.0f, 1.0f, 0.0f)));
}

HYP_ENGINE_TEST(SceneGraph, AddChildKeepWorldTransformPreservesWorldPosition)
{
    Handle<Scene> scene = CreateTestScene("AddChildKeepWorldTransformPreservesWorldPosition");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> parent = CreateNode("Parent", Vec3f(5.0f, 0.0f, 0.0f));
    Handle<Node> child = CreateNode("Child", Vec3f(10.0f, 0.0f, 0.0f));

    root->AddChild(parent);
    root->AddChild(child);

    parent->AddChildKeepWorldTransform(child);

    HYP_TEST_CHECK(child->GetParent() == parent.Get());
    HYP_TEST_CHECK(Near(child->GetWorldTranslation(), Vec3f(10.0f, 0.0f, 0.0f)));
    HYP_TEST_CHECK(Near(child->GetLocalTranslation(), Vec3f(5.0f, 0.0f, 0.0f)));
}

HYP_ENGINE_TEST(SceneGraph, AddChildWithoutKeepingWorldTransformKeepsLocal)
{
    Handle<Scene> scene = CreateTestScene("AddChildWithoutKeepingWorldTransformKeepsLocal");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> parent = CreateNode("Parent", Vec3f(5.0f, 0.0f, 0.0f));
    Handle<Node> child = CreateNode("Child", Vec3f(10.0f, 0.0f, 0.0f));

    root->AddChild(parent);
    parent->AddChild(child);

    HYP_TEST_CHECK(Near(child->GetLocalTranslation(), Vec3f(10.0f, 0.0f, 0.0f)));
    HYP_TEST_CHECK(Near(child->GetWorldTranslation(), Vec3f(15.0f, 0.0f, 0.0f)));
}

HYP_ENGINE_TEST(SceneGraph, RemovedNodeLosesParentTransform)
{
    Handle<Scene> scene = CreateTestScene("RemovedNodeLosesParentTransform");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> parent = CreateNode("Parent", Vec3f(5.0f, 0.0f, 0.0f));
    Handle<Node> child = CreateNode("Child", Vec3f(1.0f, 0.0f, 0.0f));

    root->AddChild(parent);
    parent->AddChild(child);

    HYP_TEST_REQUIRE(Near(child->GetWorldTranslation(), Vec3f(6.0f, 0.0f, 0.0f)));

    child->Remove(/* moveToDetached */ false);

    HYP_TEST_CHECK(Near(child->GetWorldTranslation(), Vec3f(1.0f, 0.0f, 0.0f)));
}

HYP_ENGINE_TEST(SceneGraph, DepthAndDeepPath)
{
    Handle<Scene> scene = CreateTestScene("DepthAndDeepPath");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> a = CreateNode("A");
    Handle<Node> b = CreateNode("B");
    Handle<Node> c = CreateNode("C");

    root->AddChild(a);
    a->AddChild(b);
    b->AddChild(c);

    HYP_TEST_CHECK(c->CalculateDepth() == b->CalculateDepth() + 1);
    HYP_TEST_CHECK(b->CalculateDepth() == a->CalculateDepth() + 1);
    HYP_TEST_CHECK(a->CalculateDepth() == root->CalculateDepth() + 1);

    HYP_TEST_CHECK(c->IsOrHasParent(a.Get()));
    HYP_TEST_CHECK(c->IsOrHasParent(c.Get()));
    HYP_TEST_CHECK(!a->IsOrHasParent(c.Get()));
    HYP_TEST_CHECK(c->FindParentWithName("A") == a.Get());
    HYP_TEST_CHECK(c->FindParentWithName("Missing") == nullptr);

    const Array<Name> deepPath = c->GetDeepPath();

    HYP_TEST_REQUIRE(deepPath.Size() >= 3);
    HYP_TEST_CHECK(deepPath[deepPath.Size() - 1] == NAME("C"));
    HYP_TEST_CHECK(deepPath[deepPath.Size() - 2] == NAME("B"));
    HYP_TEST_CHECK(deepPath[deepPath.Size() - 3] == NAME("A"));
}

HYP_ENGINE_TEST(SceneGraph, SelectFindsNestedNodes)
{
    Handle<Scene> scene = CreateTestScene("SelectFindsNestedNodes");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> a = CreateNode("A");
    Handle<Node> b = CreateNode("B");
    Handle<Node> c = CreateNode("C");

    root->AddChild(a);
    a->AddChild(b);
    b->AddChild(c);

    HYP_TEST_CHECK(root->Select("A") == a);
    HYP_TEST_CHECK(root->Select("A/B") == b);
    HYP_TEST_CHECK(root->Select("A/B/C") == c);
    HYP_TEST_CHECK(a->Select("B/C") == c);
    HYP_TEST_CHECK(!root->Select("A/Missing").IsValid());
    HYP_TEST_CHECK(!root->Select("a").IsValid());
}

HYP_ENGINE_TEST(SceneGraph, FindChildByNameAndUUID)
{
    Handle<Scene> scene = CreateTestScene("FindChildByNameAndUUID");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> first = CreateNode("First");
    Handle<Node> second = CreateNode("Second");

    root->AddChild(first);
    root->AddChild(second);

    HYP_TEST_CHECK(root->FindChildByName(StringHash("Second")) == second.Get());
    HYP_TEST_CHECK(root->FindChildByName(StringHash("Missing")) == nullptr);
    HYP_TEST_CHECK(root->FindChildByUUID(first->GetUUID()) == first.Get());
    HYP_TEST_CHECK(root->FindChildByUUID(UUID()) == nullptr);
    HYP_TEST_CHECK(first->GetUUID() != second->GetUUID());
}

HYP_ENGINE_TEST(SceneGraph, DescendantsVisitEveryNode)
{
    Handle<Scene> scene = CreateTestScene("DescendantsVisitEveryNode");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> a = CreateNode("A");
    Handle<Node> b = CreateNode("B");
    Handle<Node> c = CreateNode("C");
    Handle<Node> d = CreateNode("D");

    root->AddChild(a);
    a->AddChild(b);
    a->AddChild(c);
    c->AddChild(d);

    const Array<Node*> descendants = a->GetDescendantsArray();

    HYP_TEST_CHECK(descendants.Size() == 3);
    HYP_TEST_CHECK(descendants.Contains(b.Get()));
    HYP_TEST_CHECK(descendants.Contains(c.Get()));
    HYP_TEST_CHECK(descendants.Contains(d.Get()));
    HYP_TEST_CHECK(!descendants.Contains(a.Get()));

    uint32 visited = 0;

    for (Node* descendant : a->GetDescendants())
    {
        HYP_TEST_CHECK(descendant != nullptr);

        ++visited;
    }

    HYP_TEST_CHECK(visited == 3);
}

HYP_ENGINE_TEST(SceneGraph, TagsAddGetRemove)
{
    Handle<Scene> scene = CreateTestScene("TagsAddGetRemove");

    Handle<Node> node = CreateNode("Tagged");
    scene->GetRoot()->AddChild(node);

    HYP_TEST_CHECK(!node->HasTag(StringHash("Health")));

    node->AddTag(NodeTag(NAME("Health"), 42));
    node->AddTag(NodeTag(NAME("Speed"), 1.5f));

    HYP_TEST_CHECK(node->HasTag(StringHash("Health")));
    HYP_TEST_CHECK(node->HasTag(StringHash("Speed")));

    const NodeTag& health = node->GetTag(StringHash("Health"));

    HYP_TEST_REQUIRE(health.data.Is<int>());
    HYP_TEST_CHECK(health.data.Get<int>() == 42);

    HYP_TEST_CHECK(node->RemoveTag(StringHash("Health")));
    HYP_TEST_CHECK(!node->HasTag(StringHash("Health")));
    HYP_TEST_CHECK(node->HasTag(StringHash("Speed")));
    HYP_TEST_CHECK(!node->RemoveTag(StringHash("Health")));
}

HYP_ENGINE_TEST(SceneGraph, CloneCopiesSubtree)
{
    Handle<Scene> scene = CreateTestScene("CloneCopiesSubtree");
    const Handle<Node>& root = scene->GetRoot();

    Handle<Node> original = CreateNode("Original", Vec3f(1.0f, 2.0f, 3.0f));
    Handle<Node> child = CreateNode("Child", Vec3f(0.0f, 1.0f, 0.0f));
    original->AddTag(NodeTag(NAME("Mark"), 7));
    original->AddChild(child);
    root->AddChild(original);

    Handle<Node> clone = original->Clone();

    HYP_TEST_REQUIRE(clone.IsValid());

    HYP_TEST_CHECK(clone != original);
    HYP_TEST_CHECK(clone->GetParent() == nullptr);
    HYP_TEST_CHECK(clone->GetName() == NAME("Original"));
    HYP_TEST_CHECK(Near(clone->GetLocalTranslation(), Vec3f(1.0f, 2.0f, 3.0f)));
    HYP_TEST_CHECK(clone->HasTag(StringHash("Mark")));
    HYP_TEST_CHECK(clone->GetUUID() != original->GetUUID());

    HYP_TEST_REQUIRE(clone->NumChildren() == 1);

    HYP_TEST_CHECK(clone->GetChild(0) != child);
    HYP_TEST_CHECK(clone->GetChild(0)->GetName() == NAME("Child"));
    HYP_TEST_CHECK(clone->GetChild(0)->GetParent() == clone.Get());

    // the original is untouched
    HYP_TEST_CHECK(original->NumChildren() == 1);
    HYP_TEST_CHECK(original->GetChild(0) == child);
    HYP_TEST_CHECK(root->NumChildren() == 1);
}

HYP_ENGINE_TEST(SceneGraph, CloneWithoutChildren)
{
    Handle<Scene> scene = CreateTestScene("CloneWithoutChildren");

    Handle<Node> original = CreateNode("Original");
    original->AddChild(CreateNode("Child"));
    scene->GetRoot()->AddChild(original);

    Handle<Node> clone = original->CloneAs(Node::StaticClass(), /* cloneChildren */ false);

    HYP_TEST_REQUIRE(clone.IsValid());
    HYP_TEST_CHECK(clone->NumChildren() == 0);
    HYP_TEST_CHECK(original->NumChildren() == 1);
}

HYP_ENGINE_TEST(SceneGraph, EntityComponentsAddGetRemove)
{
    Handle<Scene> scene = CreateTestScene("EntityComponentsAddGetRemove");

    Handle<Entity> entity = MakeHandle<Entity>();
    scene->GetRoot()->AddChild(entity);

    // every entity in a scene starts with a transform
    HYP_TEST_CHECK(entity->HasComponent<TransformComponent>());

    HYP_TEST_CHECK(!entity->HasComponent<InstanceClusterComponent>());
    HYP_TEST_CHECK(entity->TryGetComponent<InstanceClusterComponent>() == nullptr);

    InstanceClusterComponent added;
    added.cell = Vec2i(3, 4);
    added.memberIndex = 7;

    entity->AddComponent<InstanceClusterComponent>(std::move(added));

    HYP_TEST_REQUIRE(entity->HasComponent<InstanceClusterComponent>());

    HYP_TEST_CHECK(entity->GetComponent<InstanceClusterComponent>().cell == Vec2i(3, 4));

    entity->GetComponent<InstanceClusterComponent>().memberIndex = 9;

    const InstanceClusterComponent* component = entity->TryGetComponent<InstanceClusterComponent>();

    HYP_TEST_REQUIRE(component != nullptr);
    HYP_TEST_CHECK(component->memberIndex == 9);

    HYP_TEST_CHECK(entity->RemoveComponent<InstanceClusterComponent>());
    HYP_TEST_CHECK(!entity->HasComponent<InstanceClusterComponent>());
    HYP_TEST_CHECK(!entity->RemoveComponent<InstanceClusterComponent>());
}

HYP_ENGINE_TEST(SceneGraph, EntityCloneCopiesSerializedComponents)
{
    Handle<Scene> scene = CreateTestScene("EntityCloneCopiesSerializedComponents");

    Handle<Entity> entity = MakeHandle<Entity>();
    entity->SetName(NAME("Source"));
    scene->GetRoot()->AddChild(entity);

    // components are cloned by serializing them, so only a component with serialized fields carries over
    CharacterModelComponent added;
    added.turnSharpness = 3.5f;
    added.forwardYawOffset = 1.25f;
    added.alignToCapsule = false;

    entity->AddComponent<CharacterModelComponent>(std::move(added));

    Handle<Node> cloneNode = entity->Clone();
    Handle<Entity> clone = DynamicCast<Entity>(cloneNode);

    HYP_TEST_REQUIRE(clone.IsValid());

    HYP_TEST_CHECK(clone != entity);
    HYP_TEST_CHECK(clone->GetName() == NAME("Source"));
    HYP_TEST_REQUIRE(clone->HasComponent<CharacterModelComponent>());

    const CharacterModelComponent& clonedComponent = clone->GetComponent<CharacterModelComponent>();

    HYP_TEST_CHECK(clonedComponent.turnSharpness == 3.5f);
    HYP_TEST_CHECK(clonedComponent.forwardYawOffset == 1.25f);
    HYP_TEST_CHECK(!clonedComponent.alignToCapsule);

    clone->GetComponent<CharacterModelComponent>().turnSharpness = 99.0f;

    HYP_TEST_CHECK(entity->GetComponent<CharacterModelComponent>().turnSharpness == 3.5f);
}

HYP_ENGINE_TEST(SceneGraph, EntityManagerOwnsEntitiesOfScene)
{
    Handle<Scene> scene = CreateTestScene("EntityManagerOwnsEntitiesOfScene");
    const Handle<EntityManager>& entityManager = scene->GetEntityManager();

    Handle<Entity> entity = MakeHandle<Entity>();
    scene->GetRoot()->AddChild(entity);

    HYP_TEST_CHECK(entityManager->HasEntity(entity.Get()));

    entity->Remove(/* moveToDetached */ false);

    HYP_TEST_CHECK(!entityManager->HasEntity(entity.Get()));
}

} // namespace

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
