/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Node.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/BVH.hpp>
#include <Scene/DetachedScene.hpp>

#include <Scene/Animation/Bone.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/ComponentInterface.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>

#include <Core/Debug/Debug.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorDelegates.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorPickCache.hpp>
#endif // HYP_EDITOR

#include <Framework/EngineDriver.hpp>
#include <Framework/GameState.hpp>

#include <Rendering/Mesh.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <cstring>

#include <Node.generated.inl>

namespace Hyperion {

ScriptableDelegate<void, Node*, bool> Node::OnChildAdded;
ScriptableDelegate<void, Node*, bool> Node::OnChildRemoved;
ScriptableDelegate<void, Node*> Node::TransformUpdated;

#ifdef HYP_EDITOR
extern Handle<EditorState> g_editorState;
#endif

#pragma region NodeTag

String NodeTag::ToString() const
{
    if (!data.IsValid())
    {
        return String::empty;
    }

    String str;

    Visit(data, [&str](const auto& value)
          {
              str = HYP_FORMAT("{}", value);
          });

    return str;
}

#pragma endregion NodeTag

#pragma region Node

Node::Node(Name name, const Transform& localTransform, Scene* scene)
    : m_name(name),
      m_nodeFlags(NodeFlags::Default),
      m_parentNode(nullptr),
      m_localTransform(localTransform),
      m_scene(scene != nullptr ? scene : GetDetachedSceneForCurrentThread()),
      m_transformLocked(false)
{
    for (Node* child : m_childNodes)
    {
        if (!child)
        {
            continue;
        }

        child->SetScene(m_scene);
    }
}

Node::~Node()
{
    for (Handle<Node>& child : m_childNodes)
    {
        if (!child.IsValid())
        {
            continue;
        }

        child->m_parentNode = nullptr;

        child.Reset();
    }

    OnChildAdded.RemoveAllForTarget(this);
    OnChildRemoved.RemoveAllForTarget(this);
    TransformUpdated.RemoveAllForTarget(this);
}

Handle<Node> Node::Clone() const
{
    const Class* nodeClass = InstanceClass();
    AssertDebug(nodeClass != nullptr);

    if (nodeClass == nullptr)
    {
        HYP_LOG(Node, Error, "Cannot clone node: class is null");
        return Handle<Node>::Null();
    }

    BoxedValue boxed;
    if (!nodeClass->CreateInstance(boxed))
    {
        HYP_LOG(Node, Error, "Failed to create instance of class {}", nodeClass->GetName());
        return Handle<Node>::Null();
    }

    if (!boxed.Is<Handle<Node>>())
    {
        HYP_LOG(Node, Error, "Created instance is not a Handle<Node>");
        return Handle<Node>::Null();
    }

    Handle<Node>& cloned = boxed.Get<Handle<Node>>();
    if (!cloned.IsValid())
    {
        return Handle<Node>::Null();
    }

    cloned->SetNodeFlags(m_nodeFlags);
    cloned->SetLocalTransform(m_localTransform, TransformChangeType::Default);
    cloned->SetLocalBounds(m_localBounds);
    cloned->SetName(m_name);

    for (const NodeTag& tag : m_tags)
    {
        cloned->AddTag(NodeTag(tag));
    }

    // Clone children recursively
    for (Node* child : m_childNodes)
    {
        if (child)
        {
            Handle<Node> clonedChild = child->Clone();
            if (clonedChild.IsValid())
            {
                cloned->AddChild(clonedChild);
            }
        }
    }

    return cloned;
}

void Node::Init()
{
    for (Node* child : m_childNodes)
    {
        InitObject(child);
    }

    SetReady(true);
}

bool Node::HasName() const
{
    return m_name.IsValid();
}

void Node::SetNodeFlags(EnumFlags<NodeFlags> flags)
{
    if (m_nodeFlags == flags)
    {
        return;
    }

    const bool wasStatic = IsStatic();

    m_nodeFlags = flags;

    const bool isStatic = IsStatic();

    if (wasStatic != isStatic)
    {
        OnMobilityChanged(isStatic);
    }

    MarkDirty();
}

bool Node::IsOrHasParent(const Node* node) const
{
    if (node == nullptr)
    {
        return false;
    }

    if (node == this)
    {
        return true;
    }

    if (m_parentNode == nullptr)
    {
        return false;
    }

    return m_parentNode->IsOrHasParent(node);
}

Node* Node::FindParentWithName(UTF8StringView name) const
{
    if (GetName() == name)
    {
        return const_cast<Node*>(this);
    }

    if (m_parentNode == nullptr)
    {
        return nullptr;
    }

    return m_parentNode->FindParentWithName(name);
}

void Node::SetScene(Scene* scene, bool moveToDetached)
{
    SetScene_Internal(scene, moveToDetached);
}

void Node::SetScene_Internal(Scene* scene, bool moveToDetached)
{
    if (moveToDetached && !scene)
    {
        scene = GetDetachedSceneForCurrentThread();
    }

    if (m_scene != scene)
    {
        m_scene = scene;

        MarkDirty();

        for (Node* child : m_childNodes)
        {
            child->SetScene_Internal(m_scene, moveToDetached);
        }
    }
}

World* Node::GetWorld() const
{
    if (m_scene != nullptr)
    {
        return m_scene->GetWorld();
    }

    return nullptr;
}

void Node::OnTransformUpdated()
{
    TransformUpdated.Fire(this, this);
}

void Node::OnMobilityChanged(bool isStatic)
{
    if (isStatic)
    {
        for (Node* child : m_childNodes)
        {
            child->SetNodeFlags(child->GetNodeFlags() | NodeFlags::MobilityStaticByProxy);
        }
    }
    else
    {
        for (Node* child : m_childNodes)
        {
            child->SetNodeFlags(child->GetNodeFlags() & ~NodeFlags::MobilityStaticByProxy);
        }
    }

    MarkDirty();
}

void Node::OnAttachedToNode(Node* node)
{
    Assert(node != nullptr);

    m_parentNode = node;

    if (m_parentNode->IsStatic())
    {
        SetNodeFlags(GetNodeFlags() | NodeFlags::MobilityStaticByProxy);
    }
    else
    {
        SetNodeFlags(GetNodeFlags() & ~NodeFlags::MobilityStaticByProxy);
    }
}

void Node::OnDetachedFromNode(Node* node)
{
    Assert(node != nullptr);

    SetNodeFlags(GetNodeFlags() & ~NodeFlags::MobilityStaticByProxy);

    m_parentNode = nullptr;
}

void Node::OnNodeAttached(Node* node)
{
    MarkDirty();
}

void Node::OnNodeDetached(Node* node)
{
    MarkDirty();
}

void Node::SetChildren(const NodeList& children)
{
    RemoveAllChildren(/* moveToDetached */ false);

    for (const Handle<Node>& child : children)
    {
        AddChild(child);
    }
}

Handle<Node> Node::AddChild(const Handle<Node>& node)
{
    if (!node.IsValid())
    {
        return AddChild(Handle<Node>(MakeHandle<Node>()));
    }

    if (node.Get() == this || node->GetParent() == this)
    {
        return node;
    }

    if (node->GetParent() != nullptr)
    {
        HYP_LOG(Node, Warning, "Attaching node {} to {} when it already has a parent node ({}). Node will be detached from parent.",
                node->GetName(), GetName(), node->GetParent()->GetName());

        node->Remove();
    }

    Assert(
        !IsOrHasParent(node.Get()),
        "Attaching node %s to %s would create a circular reference",
        node->GetName().LookupString(),
        GetName().LookupString());

    InitObject(node);

    bool wasTransformLocked = false;

    if (node->IsTransformLocked())
    {
        wasTransformLocked = true;

        node->UnlockTransform();
    }

    node->SetScene(m_scene);
    node->OnAttachedToNode(this);
    node->UpdateWorldTransform();

    if (wasTransformLocked)
    {
        node->LockTransform();
    }

    m_childNodes.PushBack(node);

    OnNodeAttached(node);

    Node* currentParent = this;

    while (currentParent != nullptr)
    {
        OnChildAdded.Fire(currentParent, node, /* direct */ currentParent == this);

        currentParent = currentParent->m_parentNode;
    }

    MarkDirty();

    return node;
}

bool Node::RemoveChild(const Node* node, bool moveToDetached)
{
    if (!node)
    {
        return false;
    }

    auto it = m_childNodes.FindIf(
        [node](Node* otherNode)
        {
            return otherNode == node;
        });

    if (it == m_childNodes.End())
    {
        return false;
    }

    Handle<Node> childNode = std::move(*it);
    m_childNodes.Erase(it);

    Assert(childNode.IsValid());
    Assert(childNode->GetParent() == this);

    bool wasTransformLocked = false;

    if (node->IsTransformLocked())
    {
        wasTransformLocked = true;

        childNode->UnlockTransform();
    }

    OnNodeDetached(childNode);

    childNode->OnDetachedFromNode(this);

    childNode->UpdateWorldTransform();

    childNode->SetScene(nullptr, moveToDetached);

    if (wasTransformLocked)
    {
        childNode->LockTransform();
    }

    Node* currentParent = this;

    while (currentParent != nullptr)
    {
        OnChildRemoved.Fire(currentParent, const_cast<Node*>(node), /* direct */ currentParent == this);

        currentParent = currentParent->m_parentNode;
    }

    UpdateWorldTransform();

    childNode.Reset();

    MarkDirty();

    return true;
}

bool Node::RemoveAt(uint32 index, bool moveToDetached)
{
    if (index >= m_childNodes.Size())
    {
        return false;
    }

    Node* childNode = m_childNodes[index];

    return RemoveChild(childNode, moveToDetached);
}

bool Node::Remove(bool moveToDetached)
{
    if (!m_parentNode)
    {
        SetScene(nullptr, moveToDetached);

        return true;
    }

    return m_parentNode->RemoveChild(this, moveToDetached);
}

void Node::RemoveAllChildren(bool moveToDetached)
{
    for (auto it = m_childNodes.begin(); it != m_childNodes.end();)
    {
        if (Node* node = *it)
        {
            Assert(node != nullptr);
            Assert(node->GetParent() == this);

            OnNodeDetached(node);

            node->OnDetachedFromNode(this);
            node->SetScene(nullptr, moveToDetached);

            Node* currentParent = this;

            while (currentParent != nullptr)
            {
                OnChildRemoved.Fire(currentParent, node, /* direct */ currentParent == this);

                currentParent = currentParent->m_parentNode;
            }
        }

        it = m_childNodes.Erase(it);
        it->Reset();
    }

    UpdateWorldTransform();

    MarkDirty();
}

uint32 Node::NumChildren() const
{
    return uint32(m_childNodes.Size());
}

Handle<Node> Node::GetChild(uint32 index) const
{
    if (index >= m_childNodes.Size())
    {
        return Handle<Node>::empty;
    }

    return m_childNodes[index];
}

Handle<Node> Node::Select(ANSIStringView selector) const
{
    Handle<Node> result;

    if (selector.Size() == 0)
    {
        return result;
    }

    char ch;

    char buffer[256] {};
    uint32 bufferIndex = 0;

    const Node* searchNode = this;

    const char* strBegin = selector.Data();
    const char* strEnd = strBegin + selector.Size();

    const char* str = strBegin;

    for (; str != strEnd && (ch = *str) != '\0'; ++str)
    {
        const char prevSelectorChar = str == strBegin ? '\0' : *(str - 1);

        if (ch == '/' && prevSelectorChar != '\\')
        {
            const auto it = searchNode->FindChild(buffer);

            if (it == searchNode->GetChildren().End())
            {
                return Handle<Node>::empty;
            }

            searchNode = it->Get();
            result = *it;

            if (!searchNode)
            {
                return Handle<Node>::empty;
            }

            bufferIndex = 0;
        }
        else if (ch != '\\')
        {
            buffer[bufferIndex] = ch;

            ++bufferIndex;

            if (bufferIndex == GetArrayCount(buffer))
            {
                HYP_LOG(Node, Warning, "Node search string too long, must be within buffer size limit of {}",
                        GetArrayCount(buffer));

                return Handle<Node>::empty;
            }
        }

        buffer[bufferIndex] = '\0';
    }

    // find remaining
    if (bufferIndex != 0)
    {
        const auto it = searchNode->FindChild(buffer);

        if (it == searchNode->GetChildren().End())
        {
            return Handle<Node>::empty;
        }

        searchNode = it->Get();
        result = *it;
    }

    return result;
}

Array<Name> Node::GetDeepPath() const
{
    Array<Name> path = { m_name };

    Node* current = m_parentNode;
    while (current != nullptr)
    {
        path.PushBack(current->GetName());
        current = current->m_parentNode;
    }

    // pop root.
    path.PopFront();
    // reverse order to have higher parents first.
    path.Reverse();

    return path;
}

Node::NodeList::Iterator Node::FindChild(const Node* node)
{
    return m_childNodes.FindIf([node](const auto& it)
                               {
                                   return it.Get() == node;
                               });
}

Node::NodeList::ConstIterator Node::FindChild(const Node* node) const
{
    return m_childNodes.FindIf([node](const auto& it)
                               {
                                   return it.Get() == node;
                               });
}

Node::NodeList::Iterator Node::FindChild(const char* name)
{
    const StringHash stringHash { name };

    return m_childNodes.FindIf([stringHash](Node* node)
                               {
                                   return node->GetName() == stringHash;
                               });
}

Node::NodeList::ConstIterator Node::FindChild(const char* name) const
{
    const StringHash stringHash { name };

    return m_childNodes.FindIf([stringHash](Node* node)
                               {
                                   return node->GetName() == stringHash;
                               });
}

Array<Node*> Node::GetDescendantsArray() const
{
    // add all children to the list
    Array<Node*> descendants;

    typedef void (*CollectFunc)(Array<Node*>& descendants, const Node& target, void* collectFunc);

    CollectFunc collect = [](Array<Node*>& descendants, const Node& target, void* collectFunc)
    {
        descendants.Reserve(descendants.Size() + target.GetChildren().Size());

        for (Node* child : target.GetChildren())
        {
            AssertDebug(child != nullptr);

            descendants.PushBack(child);

            (*static_cast<CollectFunc*>(collectFunc))(descendants, *child, collectFunc);
        }
    };

    collect(descendants, *this, &collect);

    return descendants;
}

void Node::LockTransform()
{
    m_transformLocked = true;

    for (Node* child : m_childNodes)
    {
        child->LockTransform();
    }
}

void Node::UnlockTransform()
{
    m_transformLocked = false;

    for (Node* child : m_childNodes)
    {
        child->UnlockTransform();
    }
}

void Node::SetLocalTransform(const Transform& transform, TransformChangeType changeType)
{
    if (IsTransformLocked())
    {
        return;
    }

    if (m_localTransform == transform)
    {
        return;
    }

    m_localTransform = transform;

    if (changeType != TransformChangeType::Simulation)
    {
        MarkDirty();
    }

    UpdateWorldTransform();
}

void Node::SetIsStatic(bool isStatic)
{
    constexpr EnumFlags<NodeFlags> ControlledMobilityFlags = NodeFlags::MobilityStatic | NodeFlags::MobilityDynamic;

    EnumFlags<NodeFlags> newNodeFlags = m_nodeFlags;

    newNodeFlags &= ~ControlledMobilityFlags;

    if (isStatic)
    {
        newNodeFlags |= NodeFlags::MobilityStatic;
    }
    else
    {
        newNodeFlags |= NodeFlags::MobilityDynamic;
    }

    if (newNodeFlags == m_nodeFlags)
    {
        return;
    }

    SetNodeFlags(newNodeFlags);
}

void Node::SetIsDynamic(bool isDynamic)
{
    SetIsStatic(!isDynamic);
}

void Node::SetLocalBounds(const BoundingBox& aabb)
{
    if (m_localBounds == aabb)
    {
        return;
    }

    m_localBounds = aabb;

    MarkDirty();
}

BoundingBox Node::GetLocalBoundsWithChildren() const
{
    if (!m_localBounds.IsFinite())
    {
        return m_localBounds;
    }

    BoundingBox aabb = m_localBounds.IsValid() ? m_localBounds : BoundingBox::Empty();

    for (Node* child : GetChildren())
    {
        if (!(child->GetNodeFlags() & NodeFlags::ExcludeFromParentBounds))
        {
            BoundingBox childBounds = child->GetLocalBoundsWithChildren();

            if (childBounds.IsValid() && childBounds.IsFinite() && !childBounds.IsZero())
            {
                aabb = aabb.Union(child->GetLocalTransform() * childBounds);
            }
        }
    }

    return aabb;
}

BoundingBox Node::GetWorldBounds() const
{
    if (!m_localBounds.IsFinite())
    {
        return m_localBounds;
    }

    BoundingBox aabb = m_localBounds.IsValid() ? (m_worldMatrix * m_localBounds) : BoundingBox::Empty();

    for (Node* child : GetChildren())
    {
        if (!(child->GetNodeFlags() & NodeFlags::ExcludeFromParentBounds))
        {
            BoundingBox childBounds = child->GetWorldBounds();

            if (childBounds.IsValid() && childBounds.IsFinite() && !childBounds.IsZero())
            {
                // Check if it would explode the scene bounds, and skip if so.
                // Nodes that match this criteria should ideally be fixed so they can properly contribute to the scene's world-space bounds.
                if (MathUtil::Max(MathUtil::Abs(childBounds.min.Min()), MathUtil::Abs(childBounds.max.Max())) > (FLT_MAX * 0.5f))
                {
                    continue;
                }

                aabb = aabb.Union(childBounds);
            }
        }
    }

    return aabb;
}

Vec3f Node::GetWorldTranslation() const
{
    Vec3f translationWS = m_localTransform.GetTranslation();

    if (m_parentNode != nullptr && !(m_nodeFlags & NodeFlags::IgnoreParentTranslation))
    {
        translationWS = m_parentNode->GetWorldMatrix().TransformVector(translationWS);
    }

    return translationWS;
}

void Node::SetWorldTranslation(const Vec3f& translation, TransformChangeType changeType)
{
    if (m_parentNode == nullptr || (m_nodeFlags & NodeFlags::IgnoreParentTranslation))
    {
        SetLocalTranslation(translation, changeType);

        return;
    }

    SetLocalTranslation(m_parentNode->GetWorldMatrix().Inverse().TransformVector(translation), changeType);
}

Vec3f Node::GetWorldScale() const
{
    Vec3f scaleWS = m_localTransform.GetScale();

    if (m_parentNode != nullptr && !(m_nodeFlags & NodeFlags::IgnoreParentScale))
    {
        scaleWS *= m_parentNode->GetWorldScale();
    }

    return scaleWS;
}

void Node::SetWorldScale(const Vec3f& scale, TransformChangeType changeType)
{
    if (m_parentNode == nullptr || (m_nodeFlags & NodeFlags::IgnoreParentScale))
    {
        SetLocalScale(scale, changeType);

        return;
    }

    SetLocalScale(scale / m_parentNode->GetWorldScale(), changeType);
}

Quat4f Node::GetWorldRotation() const
{
    return Quat4f(m_worldMatrix).Inverse();
}

void Node::SetWorldRotation(const Quat4f& rotation, TransformChangeType changeType)
{
    if (m_parentNode == nullptr || (m_nodeFlags & NodeFlags::IgnoreParentRotation))
    {
        SetLocalRotation(rotation, changeType);

        return;
    }

    SetLocalRotation(m_parentNode->GetWorldRotation().Inverse() * rotation, changeType);
}

void Node::UpdateWorldTransform(bool updateChildTransforms)
{
    if (IsTransformLocked())
    {
        return;
    }

    if (IsA<Bone>())
    {
        static_cast<Bone*>(this)->UpdateBoneTransform(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    const Mat4f prevWorldMatrix = m_worldMatrix;

    Mat4f transformMatrix = m_localTransform.GetMatrix();

    if (m_parentNode != nullptr)
    {
        transformMatrix = m_parentNode->GetWorldMatrix() * transformMatrix;

        if (m_nodeFlags & NodeFlags::IgnoreParentTransform)
        {
            if (m_nodeFlags & NodeFlags::IgnoreParentTranslation)
            {
                transformMatrix[3][0] = prevWorldMatrix[3][0];
                transformMatrix[3][1] = prevWorldMatrix[3][1];
                transformMatrix[3][2] = prevWorldMatrix[3][2];
                transformMatrix[3][3] = 1.0f;
            }

            if (m_nodeFlags & NodeFlags::IgnoreParentRotation)
            {
                const Mat4f curr = transformMatrix;

                transformMatrix = prevWorldMatrix;

                transformMatrix[3][0] = curr[3][0];
                transformMatrix[3][1] = curr[3][1];
                transformMatrix[3][2] = curr[3][2];
                transformMatrix[3][3] = curr[3][3];

                transformMatrix.Orthonormalize();
            }

            if (m_nodeFlags & NodeFlags::IgnoreParentScale)
            {
                const Vec3f prevScale = prevWorldMatrix.ExtractScale();
                const Vec3f currentScale = transformMatrix.ExtractScale();

                const Vec3f denom = prevScale / currentScale;

                transformMatrix[0][0] *= denom.x;
                transformMatrix[0][1] *= denom.x;
                transformMatrix[0][2] *= denom.x;

                transformMatrix[1][0] *= denom.y;
                transformMatrix[1][1] *= denom.y;
                transformMatrix[1][2] *= denom.y;

                transformMatrix[2][0] *= denom.z;
                transformMatrix[2][1] *= denom.z;
                transformMatrix[2][2] *= denom.z;

                transformMatrix.Orthonormalize();
            }
        }
    }

    m_worldMatrix = transformMatrix;

    if (updateChildTransforms)
    {
        for (Node* node : m_childNodes)
        {
            node->UpdateWorldTransform(true);
        }
    }

    if (m_worldMatrix == prevWorldMatrix)
    {
        return;
    }

    OnTransformUpdated();
}

uint32 Node::CalculateDepth() const
{
    uint32 depth = 0;

    Node* parent = m_parentNode;

    while (parent != nullptr)
    {
        ++depth;
        parent = parent->GetParent();
    }

    return depth;
}

uint32 Node::FindSelfIndex() const
{
    if (m_parentNode == nullptr)
    {
        return ~0u;
    }

    const auto it = m_parentNode->FindChild(this);

    if (it == m_parentNode->GetChildren().End())
    {
        return 0;
    }

    return uint32(it - m_parentNode->GetChildren().Begin());
}

bool Node::TestRay(const Ray& ray, RayTestResults& outResults, EnumFlags<RayTestFlags> flags) const
{
    const BoundingBox worldAabb = GetWorldBounds();

    bool hasEntityHit = false;

    const bool worldAabbHit = ray.TestAABB(worldAabb).HasValue();

    {
        const Class* entityClass = Entity::StaticClass();
        if (IsA(entityClass))
        {
            TSharedResLock<AssetObject> resGuard;
            Mesh* mesh = nullptr;

#ifdef HYP_EDITOR
            EditorPickCacheEntry* pickCacheEntry = nullptr;
#endif

            const Entity* entity = static_cast<const Entity*>(this);

            const BVHNode* bvh = nullptr;
            Mat4f modelMatrix = Mat4f::Identity();

            if (flags & RayTestFlags::TestBVH)
            {
                if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->mesh.IsValid())
                {
                    mesh = meshComponent->mesh;

                    if (meshComponent->mesh->GetBVH().IsValid())
                    {
                        bvh = &meshComponent->mesh->GetBVH();

#ifdef HYP_EDITOR
                        if (flags & RayTestFlags::EditorPick)
                        {
                            pickCacheEntry = g_editorState->GetPickCache().GetEntry(meshComponent->mesh);
                        }

                        if (!pickCacheEntry)
#endif
                            resGuard.Reset(*meshComponent->mesh);
                    }
                }

                modelMatrix = entity->GetWorldMatrix();
            }

            // The BVH carries its own always-current root AABB (rebuilt from live vertex
            // data whenever the mesh changes), so it's authoritative for rejection - don't
            // additionally gate on worldAabb, which is derived from the entity's separately
            // maintained local bounds and can be stale relative to the actual geometry.
            if (bvh)
            {
                RayTestResults localBvhResults;

                const Ray localSpaceRay = modelMatrix.Inverse() * ray;

#ifdef HYP_EDITOR
                if ((flags & RayTestFlags::EditorPick) && pickCacheEntry)
                {
                    localBvhResults = bvh->TestRay(
                        localSpaceRay,
                        pickCacheEntry->positions.ToSpan(),
                        pickCacheEntry->indices.ToSpan());
                }

                else
#endif
                {
                    AssertDebug(resGuard);

                    const VertexArrayView vertexData = mesh->GetVertexData(0);
                    const Span<const ubyte> indexData = mesh->GetIndexData(0);

                    // @TODO fix for non-uint32 indices
                    localBvhResults = bvh->TestRay(
                        localSpaceRay,
                        vertexData,
                        Span<const uint32>(reinterpret_cast<const uint32*>(indexData.Data()), indexData.Size() / sizeof(uint32)));
                }

                if (localBvhResults.Any())
                {
                    const Mat4f normalMatrix = modelMatrix.Transpose().Inverse();

                    RayTestResults bvhResults;

                    for (RayHit hit : localBvhResults)
                    {
                        hit.id = Id().Value();
                        hit.node = const_cast<Node*>(this);

                        Vec4f transformedNormal = normalMatrix.TransformVector(Vec4f(hit.normal, 0.0f));
                        hit.normal = transformedNormal.GetXYZ().Normalized();

                        Vec4f transformedPosition = modelMatrix.TransformVector(Vec4f(hit.hitpoint, 1.0f));
                        transformedPosition /= transformedPosition.w;

                        hit.hitpoint = transformedPosition.GetXYZ();

                        hit.distance = (hit.hitpoint - ray.position).Length();

                        bvhResults.AddHit(hit);
                    }

                    outResults.Merge(std::move(bvhResults));

                    hasEntityHit = true;
                }
            }
            else if (worldAabbHit)
            {
                hasEntityHit = ray.TestAABB(worldAabb, entity->Id().Value(), outResults);
            }
        }
    }

    if (worldAabbHit)
    {
        for (Node* childNode : m_childNodes)
        {
            if (!childNode)
            {
                continue;
            }

            if (childNode->TestRay(ray, outResults, flags))
            {
                hasEntityHit = true;
            }
        }
    }

    return hasEntityHit;
}

const Handle<Node>& Node::FindChildByName(StringHash name) const
{
    // breadth-first search
    Queue<const Node*> queue;
    queue.Push(this);

    while (queue.Any())
    {
        const Node* parent = queue.Pop();

        for (const Handle<Node>& child : parent->GetChildren())
        {
            if (!child)
            {
                continue;
            }

            if (child->GetName() == name)
            {
                return child;
            }

            queue.Push(child.Get());
        }
    }

    return Handle<Node>::Null();
}

void Node::AddTag(NodeTag&& value)
{
    if (!m_tags.Contains(value.name))
    {
        m_tags.Add(std::move(value));

        MarkDirty();
    }
}

bool Node::RemoveTag(StringHash key)
{
    bool removed = m_tags.Remove(key);

    if (removed)
    {
        MarkDirty();

        return true;
    }

    return false;
}

const NodeTag& Node::GetTag(StringHash key) const
{
    static const NodeTag emptyTag = NodeTag();

    const NodeTag* tag = m_tags.Get(key);

    return tag != nullptr ? *tag : emptyTag;
}

bool Node::HasTag(StringHash key) const
{
    return m_tags.Has(key);
}

#ifdef HYP_EDITOR
void Node::MarkDirty()
{
    if (World* world = GetWorld(); world && world->GetGameState().IsSimulating())
    {
        return;
    }

    if (m_scene != nullptr)
    {
        m_scene->MarkDirty();
    }
}
#endif // HYP_EDITOR

#pragma endregion Node

} // namespace Hyperion
