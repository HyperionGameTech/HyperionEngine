/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <UI/UIStage.hpp>
#include <UI/UIButton.hpp>
#include <UI/UIText.hpp>

#include <UI/Camera/UICameraController.hpp>

#include <Asset/Assets.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <UI/Font/FontAtlas.hpp>

#include <Rendering/Texture.hpp>

#include <System/AppContext.hpp>
#include <Input/Event.hpp>

#include <Core/Threading/Threads.hpp>

#include <Input/InputManager.hpp>

#include <Framework/EngineDriver.hpp>

#include <UIStage.generated.inl>

namespace Hyperion {

static constexpr float MinHoldTimeToDrag = 0.05f;

static HYP_FORCE_INLINE float GetTimeToNextKeyDownEvent(const UIObjectKeyState& keyState)
{
    if (keyState.count <= 1)
    {
        return 0.25f;
    }

    return 0.05f;
}

static HYP_FORCE_INLINE bool ShouldTriggerKeyDownEvent(const UIObjectKeyState& keyState)
{
    return GetTimeToNextKeyDownEvent(keyState) < keyState.heldTime;
}

ENGINE_API HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIStageUpdateManager

UIStageUpdateManager::UIStageUpdateManager(UIStage* stage)
    : UIUpdateManager(),
      m_stage(stage)
{
    Assert(stage != nullptr);
}

void UIStageUpdateManager::ProcessUpdates(float delta)
{
    HYP_SCOPE;

    if (!m_stage)
    {
        HYP_LOG(UI, Warning, "UIStageUpdateManager: Stage is null, cannot process frame updates");
        return;
    }

    UIUpdateManager::ProcessUpdates(delta);
}

void UIStageUpdateManager::RegisterForUpdate(UIObject* object, EnumFlags<UIObjectUpdateType> updateTypes)
{
    HYP_SCOPE;

    if (!object)
    {
        HYP_LOG(UI, Warning, "UIStageUpdateManager: Attempted to register null object for update");
        return;
    }

    if (!m_stage)
    {
        HYP_LOG(UI, Warning, "UIStageUpdateManager: Stage is null, cannot register object for update");
        return;
    }

    // Ensure the object belongs to this stage's hierarchy
    UIStage* objectStage = object->GetStage();
    if (objectStage != nullptr && objectStage != m_stage && !objectStage->IsOrHasParent(m_stage))
    {
        HYP_LOG(UI, Warning, "UIStageUpdateManager: Object {} does not belong to this stage hierarchy", object->GetName());
        return;
    }

    UIUpdateManager::RegisterForUpdate(object, updateTypes);
}

#pragma endregion UIStageUpdateManager

#pragma region UIStage

UIStage::UIStage()
    : UIStage(nullptr, ThreadId::Current())
{
}

UIStage::UIStage(World* world, ThreadId ownerThreadId)
    : UIObject(ownerThreadId),
      m_updateManager(this),
      m_surfaceSize { 1000, 1000 },
      m_world(world)
{
    SetName(NAME("Stage"));
    m_size = UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT });
}

UIStage::~UIStage()
{
    m_onCurrentWindowChangedHandler.Reset();
    m_onWindowResizedHandler.Reset();

    if (m_scene.IsValid())
    {
        if (IsOnThread(m_scene->GetOwnerThreadId()))
        {
            m_scene->RemoveFromWorld();
        }
        else
        {
            GetThreadById(m_scene->GetOwnerThreadId())->GetScheduler().Enqueue([scene = m_scene]()
                {
                    scene->RemoveFromWorld();
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

void UIStage::UpdateCameraControllerStack()
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (!m_camera)
    {
        return;
    }

    Handle<UICameraController> currentController;

    int controllerIndex = -1;

    const auto& controllers = m_camera->GetCameraControllers();

    for (int i = 0; i < int(controllers.Size()); i++)
    {
        const Handle<CameraController>& controller = controllers[i];

        if (controller->IsA<UICameraController>())
        {
            currentController = DynamicCast<UICameraController>(controller);
            controllerIndex = i;
            break;
        }
    }

    if (currentController)
    {
        m_camera->RemoveCameraController(currentController);
    }

    Handle<UICameraController> newController = MakeHandle<UICameraController>(
        0.0f, -float(m_surfaceSize.x),
        0.0f, float(m_surfaceSize.y));

    m_camera->SetNearClip(MinDepth);
    m_camera->SetFarClip(MaxDepth);

    m_camera->AddCameraController(newController, controllerIndex);
}

void UIStage::SetSurfaceSize(Vec2i surfaceSize)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_contentScaleFactor = (g_appContext != nullptr && g_appContext->GetMainWindow() != nullptr)
        ? g_appContext->GetMainWindow()->GetContentScaleFactor()
        : 1.0f;

    m_surfaceSize = Vec2i(Vec2f(surfaceSize) / m_contentScaleFactor);

    if (m_camera.IsValid())
    {
        m_camera->SetDimensions(surfaceSize);

        UpdateCameraControllerStack();
    }

    UpdateSize(true);
    UpdatePosition(true);
}

void UIStage::SetUIScaleFactor(float scaleFactor)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (scaleFactor <= 0.0f)
    {
        HYP_LOG(UI, Warning, "UIStage::SetUIScaleFactor: Scale factor must be greater than 0, ignoring value {}", scaleFactor);
        return;
    }

    if (MathUtil::ApproxEqual(m_uiScaleFactor, scaleFactor))
    {
        return;
    }

    m_uiScaleFactor = scaleFactor;

    // Trigger updates for all children to recalculate sizes and positions
    UpdateSize(true);
    UpdatePosition(true);
}

Scene* UIStage::GetScene() const
{
    if (Scene* uiObjectScene = UIObject::GetScene())
    {
        return uiObjectScene;
    }

    return m_scene.Get();
}

void UIStage::SetScene(const Handle<Scene>& scene)
{
    Handle<Scene> newScene = scene;

    if (!newScene.IsValid())
    {
        const ThreadId ownerThreadId = m_scene.IsValid() ? m_scene->GetOwnerThreadId() : ThreadId::Current();

        newScene = MakeHandle<Scene>(NAME_FMT("UIStage_{}_Scene", GetName()), ownerThreadId, SceneFlags::FOREGROUND | SceneFlags::UI);
        newScene->SetIsTransient(true);

        Handle<Entity> entity = MakeHandle<Entity>();
        entity->SetName(NAME_FMT("UIStage_{}_Root", GetName()));
        newScene->SetRoot(entity);
    }

    if (newScene == m_scene)
    {
        return;
    }

    if (m_scene)
    {
        Handle<Node> currentRootNode;

        currentRootNode = m_scene->GetRoot();
        Assert(currentRootNode.IsValid());

        currentRootNode->Remove();

        newScene->SetRoot(std::move(currentRootNode));

        m_scene->RemoveFromWorld();
        m_scene.Reset();
    }

    Handle<Node> cameraNode = newScene->GetRoot()->AddChild();
    cameraNode->SetName(NAME_FMT("{}_Camera", GetName()));
    cameraNode->AddChild(m_camera);

    m_scene = std::move(newScene);

    if (m_world != nullptr)
    {
        m_world->AddScene(m_scene);
    }
}

void UIStage::SetWorld(World* world)
{
    AssertOnOwnerThread();

    if (world == m_world)
    {
        return;
    }

    if (m_scene.IsValid())
    {
        m_scene->RemoveFromWorld();
        m_scene->Shutdown();

        AssertDebug(StaticCast<Entity>(m_node)->GetEntityManager() == nullptr);
    }

    m_world = world;

    if (m_world != nullptr && m_scene.IsValid())
    {
        // Set root on the Scene, shutting down the scene unsets root
        m_scene->SetRoot(m_node);

        // Calls Scene::Initialize()
        m_world->AddScene(m_scene);
    }
}

const Handle<FontAtlas>& UIStage::GetDefaultFontAtlas() const
{
    HYP_SCOPE;

    if (m_defaultFontAtlas != nullptr)
    {
        return m_defaultFontAtlas;
    }

    // Parent stage
    if (m_stage != nullptr)
    {
        return m_stage->GetDefaultFontAtlas();
    }

    return m_defaultFontAtlas;
}

void UIStage::SetDefaultFontAtlas(const Handle<FontAtlas>& fontAtlas)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_defaultFontAtlas = fontAtlas;

    OnFontAtlasUpdate();
    // OnTextSizeUpdate();
}

void UIStage::Init()
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_camera = MakeHandle<Camera>();
    m_camera->SetName(NAME_FMT("{}_UIStage_Camera", GetName()));
    m_camera->SetDirection(-Vec3f::UnitZ());

    m_camera->AddCameraController(MakeHandle<UICameraController>(
        0.0f, float(m_surfaceSize.x),
        0.0f, float(m_surfaceSize.y)));

    m_camera->SetNearClip(float(MinDepth));
    m_camera->SetFarClip(float(MaxDepth));

    InitObject(m_camera);

    const auto UpdateSurfaceSize = [this](ApplicationWindow* window)
    {
        m_onWindowResizedHandler.Reset();

        if (window == nullptr)
        {
            return;
        }

        m_contentScaleFactor = window->GetContentScaleFactor();
        const Vec2i physicalSize = window->GetSize();

        m_surfaceSize = Vec2i(Vec2f(physicalSize) / m_contentScaleFactor);

        if (m_camera.IsValid())
        {
            m_camera->SetDimensions(physicalSize);

            UpdateCameraControllerStack();
        }

        m_onWindowResizedHandler = window->OnWindowSizeChanged.BindThreaded(
            window,
            [weakThis = MakeWeakRef(this)](Vec2i newSize)
            {
                Handle<UIStage> strongThis = weakThis.Lock();

                if (!strongThis.IsValid())
                {
                    return;
                }

                strongThis->SetSurfaceSize(newSize);
            },
            g_simThread);
    };

    UpdateSurfaceSize(g_appContext->GetMainWindow());

    m_onCurrentWindowChangedHandler = g_appContext->OnCurrentWindowChanged
        .BindThreaded(g_appContext, UpdateSurfaceSize, g_simThread);

    // Will create a new Scene
    SetScene(nullptr);

    SetNodeProxy(m_scene->GetRoot());

    UIObject::Init();
}

void UIStage::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (!uiObject)
    {
        return;
    }

    UIObject::AddChildUIObject(uiObject);

    // Check if no parent stage
    if (m_stage == nullptr)
    {
        // Set child object stage to this
        uiObject->SetStage(this);
    }
}

void UIStage::Update_Internal(float delta)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_updateManager.ProcessUpdates(delta);

    UIObject::Update_Internal(delta);

    for (auto& it : m_objectMouseStates)
    {
        it.second.heldTime += delta;
    }
    for (auto& it : m_keyedDownObjects)
    {
        for (auto& jt : it)
        {
            jt.second.heldTime += delta;
        }
    }
}

void UIStage::OnAttached_Internal(UIObject* parent)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    Assert(parent != nullptr);
    Assert(parent->GetNode() != nullptr);

    // Set root to be empty node proxy, now that it is attached to another object.
    m_scene->SetRoot(Handle<Node>::empty);

    OnAttached();
}

void UIStage::OnRemoved_Internal()
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    // Re-set scene root to be our node proxy
    m_scene->SetRoot(m_node);

    OnRemoved();
}

void UIStage::SetStage_Internal(UIStage* stage)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_stage = stage;

    // Do not update children
}

bool UIStage::TestRay(const Vec2f& position, Array<Handle<UIObject>>& outObjects, EnumFlags<UIRayTestFlags> flags)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    const Vec4f worldPosition = Vec4f(position.x * float(GetSurfaceSize().x), position.y * float(GetSurfaceSize().y), 0.0f, 1.0f);
    const Vec3f direction { worldPosition.x / worldPosition.w, worldPosition.y / worldPosition.w, 0.0f };

    Ray ray;
    ray.position = worldPosition.GetXYZ() / worldPosition.w;
    ray.direction = direction;

    RayTestResults rayTestResults;

    for (auto [entity, uiComponent, transformComponent, boundingBoxComponent] : m_scene->GetEntityManager()->GetEntitySet<UIComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        UIObject* uiObject = uiComponent.uiObject.GetUnsafe();

        if (!uiObject)
        {
            continue;
        }

        if ((flags & UIRayTestFlags::ONLY_VISIBLE) && !uiObject->GetComputedVisibility())
        {
            continue;
        }

        BoundingBox aabb = uiObject->GetAABBClamped();
        aabb.min.z = -1.0f;
        aabb.max.z = 1.0f;

        if (aabb.ContainsPoint(direction))
        {
            RayHit hit {};
            hit.hitpoint = Vec3f { position.x, position.y, 0.0f };
            hit.distance = -float(uiObject->GetComputedDepth());
            hit.id = entity->Id().Value();
            hit.node = entity;

            rayTestResults.AddHit(hit);
        }
    }

    outObjects.Reserve(rayTestResults.Size());

    for (const RayHit& hit : rayTestResults)
    {
        if (Entity* entity = DynamicCast<Entity>(hit.node))
        {
            UIComponent* uiComponent = entity->TryGetComponent<UIComponent>();

            if (uiComponent != nullptr)
            {
                Handle<UIObject> uiObject = uiComponent->uiObject.Lock();

                if (uiObject.IsValid())
                {
                    outObjects.PushBack(std::move(uiObject));
                }
            }
        }
    }

    return outObjects.Any();
}

Handle<UIObject> UIStage::GetUIObjectForEntity(const Entity* entity) const
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (UIComponent* uiComponent = m_scene->GetEntityManager()->TryGetComponent<UIComponent>(entity))
    {
        if (uiComponent->uiObject != nullptr)
        {
            return MakeStrongRef(uiComponent->uiObject);
        }
    }

    return Handle<UIObject>::empty;
}

void UIStage::SetFocusedObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (uiObject == m_focusedObject)
    {
        return;
    }

    if (Handle<UIStage> parentStage = GetClosestParentUIObject<UIStage>(); parentStage.IsValid())
    {
        parentStage->SetFocusedObject(uiObject);
    }

    Handle<UIObject> currentFocusedUiObject = m_focusedObject.Lock();

    // Be sure to set the focused object to nullptr before calling Blur() to prevent infinite recursion
    // due to Blur() calling SetFocusedObject() again.
    m_focusedObject.Reset();

    if (currentFocusedUiObject != nullptr)
    {
        // Only blur children if
        const bool shouldBlurChildren = uiObject == nullptr || !uiObject->IsOrHasParent(currentFocusedUiObject);

        currentFocusedUiObject->Blur(shouldBlurChildren);
    }

    m_focusedObject = uiObject;
}

void UIStage::ComputeActualSize(const UIObjectSize& inSize, Vec2i& outActualSize, UpdateSizePhase phase, bool isInner)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    // stage with a parent stage: treat self like a normal UIObject
    if (m_stage != nullptr)
    {
        UIObject::ComputeActualSize(inSize, outActualSize, phase, isInner);

        return;
    }

    // inner calculation is the same
    if (isInner)
    {
        UIObject::ComputeActualSize(inSize, outActualSize, phase, isInner);

        return;
    }

    outActualSize = m_surfaceSize;
}

static constexpr float TouchScrollSlop = 12.0f;

UIEventHandlerResult UIStage::OnInputEvent(const Event& event)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    UIEventHandlerResult eventHandlerResult = UIEventHandlerResult::OK;

    RayTestResults rayTestResults;

    InputManager* inputManager = event.GetWindow() ? event.GetWindow()->GetInputManager() : nullptr;
    AssertDebug(inputManager != nullptr);

    if (!inputManager)
    {
        return {};
    }

    const Vec2f previousMousePosition = ToLogicalCoords(Vec2f(inputManager->GetPreviousMousePosition()));

    switch (event.GetType())
    {
    case EventType::WINDOW_FOCUS_LOST:
    {
        const Vec2f mousePosition = ToLogicalCoords(Vec2f(inputManager->GetMousePosition()));

        // when window focus gets lost we want to unset hover for anything marked as being hovered
        for (auto it = m_hoveredUiObjects.Begin(); it != m_hoveredUiObjects.End(); ++it)
        {
            if (Handle<UIObject> uiObject = it->Lock(); uiObject.IsValid())
            {
                auto mouseStatesIt = m_objectMouseStates.Find(uiObject);
                if (mouseStatesIt != m_objectMouseStates.End())
                {
                    EnumFlags<MouseButtonState>& stateMouseButtons = mouseStatesIt->second.mouseButtons;

                    if (stateMouseButtons != MouseButtonState::NONE) // no overlap; skip
                    {
                        // trigger mouse up
                        uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::PRESSED);

                        UIEventHandlerResult currentResult = OnMouseUp.Fire(uiObject.Get(), MouseEvent {
                            .baseEvent = &event,
                            .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                            .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                            .absolutePos = mousePosition,
                            .absolutePrevPos = previousMousePosition,
                            .mouseButtons = stateMouseButtons
                        });

                        eventHandlerResult |= currentResult;
                        mouseStatesIt = m_objectMouseStates.Erase(mouseStatesIt);
                    }
                }

                uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::HOVER);

                OnMouseLeave.Fire(uiObject, MouseEvent {
                    .baseEvent = &event,
                    .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                    .absolutePos = mousePosition,
                    .absolutePrevPos = previousMousePosition,
                    .mouseButtons = inputManager->GetButtonStates()
                });
            }
        }

        m_hoveredUiObjects.Clear();

        // same for keyed down objects
        for (auto it = m_keyedDownObjects.Begin(); it != m_keyedDownObjects.End(); ++it)
        {
            const KeyCode keyCode = KeyCode(m_keyedDownObjects.IndexOf(it));

            for (auto& jt : *it)
            {
                WeakHandle<UIObject>& weakUiObject = jt.first;

                if (Handle<UIObject> uiObject = weakUiObject.Lock(); uiObject.IsValid())
                {
                    OnKeyUp.Fire(uiObject, KeyboardEvent {
                        .baseEvent = &event,
                        .inputManager = inputManager,
                        .keyCode = keyCode
                    });
                }
            }
        }

        m_keyedDownObjects.Clear();

        break;
    }
    case EventType::MOUSEMOTION:
    {
        const EnumFlags<MouseButtonState> mouseButtons = inputManager->GetButtonStates();

        const Vec2f mousePosition = ToLogicalCoords(event.IsAbsoluteMousePosition()
            ? event.GetMousePosition()
            : event.GetMousePositionDeltas() + previousMousePosition);

        eventHandlerResult |= HandlePointerMove(event, mousePosition, previousMousePosition, mouseButtons);

        break;
    }
    case EventType::MOUSEBUTTON_DOWN:
    {
        const Vec2f mousePosition = ToLogicalCoords(Vec2f(inputManager->GetMousePosition()));

        eventHandlerResult |= HandlePointerDown(event, mousePosition, previousMousePosition, event.GetMouseButtons());

        break;
    }
    case EventType::MOUSEBUTTON_UP:
    {
        const Vec2f mousePosition = ToLogicalCoords(Vec2f(inputManager->GetMousePosition()));

        eventHandlerResult |= HandlePointerUp(event, mousePosition, previousMousePosition, event.GetMouseButtons(), inputManager->GetButtonStates(), /* allowClick */ true);

        break;
    }
    case EventType::TOUCH_DOWN:
    {
        if (m_primaryTouchPointerId != -1)
        {
            break;
        }

        m_primaryTouchPointerId = event.GetTouchPointerId();

        const Vec2f mousePosition = ToLogicalCoords(event.GetTouchPosition());

        m_touchGestureOrigin = mousePosition;
        m_touchLastPosition = mousePosition;
        m_touchIsScrolling = false;
        m_touchScrollTarget = WeakHandle<UIObject>();
        m_touchScrollRemainder = Vec2f::Zero();
        m_touchDownRayTestResults.Clear();

        TestRay(mousePosition / Vec2f(m_surfaceSize), m_touchDownRayTestResults);

        eventHandlerResult |= HandlePointerDown(event, mousePosition, mousePosition, MouseButtonState::LEFT);

        break;
    }
    case EventType::TOUCH_MOVE:
    {
        if (event.GetTouchPointerId() != m_primaryTouchPointerId)
        {
            break;
        }

        const Vec2f mousePosition = ToLogicalCoords(event.GetTouchPosition());
        const Vec2f prevPosition = m_touchLastPosition;
        m_touchLastPosition = mousePosition;

        if (!m_touchIsScrolling)
        {
            const Vec2f totalDelta = mousePosition - m_touchGestureOrigin;

            if (MathUtil::Abs(totalDelta.x) > TouchScrollSlop || MathUtil::Abs(totalDelta.y) > TouchScrollSlop)
            {
                const bool horizontalDominant = MathUtil::Abs(totalDelta.x) > MathUtil::Abs(totalDelta.y);
                const ScrollAxis dominantAxis = horizontalDominant ? SA_HORIZONTAL : SA_VERTICAL;

                Handle<UIObject> scrollTarget;

                for (const Handle<UIObject>& uiObject : m_touchDownRayTestResults)
                {
                    if (uiObject && uiObject->CanScrollOnAxis(dominantAxis))
                    {
                        scrollTarget = uiObject;

                        break;
                    }
                }

                if (scrollTarget)
                {
                    m_touchIsScrolling = true;
                    m_touchScrollTarget = scrollTarget;

                    // Cancel the press on whatever was pressed down without triggering a click
                    HandlePointerUp(event, mousePosition, prevPosition, MouseButtonState::LEFT, MouseButtonState::NONE, /* allowClick */ false);
                }
            }
        }

        if (m_touchIsScrolling)
        {
            if (Handle<UIObject> uiObject = m_touchScrollTarget.Lock(); uiObject.IsValid())
            {
                const Vec2f delta = mousePosition - prevPosition + m_touchScrollRemainder;
                const Vec2i wheel = Vec2i(int32(delta.x), int32(delta.y));
                m_touchScrollRemainder = delta - Vec2f(wheel);

                UIEventHandlerResult scrollResult = OnScroll.Fire(uiObject, MouseEvent {
                    .baseEvent = &event,
                    .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .relativePrevPos = uiObject->TransformScreenCoordsToRelative(prevPosition),
                    .absolutePos = mousePosition,
                    .absolutePrevPos = prevPosition,
                    .mouseButtons = MouseButtonState::LEFT,
                    .wheel = wheel,
                    .isTouch = true
                });

                eventHandlerResult |= scrollResult;
            }
            else
            {
                m_touchIsScrolling = false;
            }
        }
        else
        {
            eventHandlerResult |= HandlePointerMove(event, mousePosition, prevPosition, MouseButtonState::LEFT);
        }

        break;
    }
    case EventType::TOUCH_UP:
    {
        if (event.GetTouchPointerId() != m_primaryTouchPointerId)
        {
            break;
        }

        const Vec2f mousePosition = ToLogicalCoords(event.GetTouchPosition());
        const Vec2f prevPosition = m_touchLastPosition;

        if (!m_touchIsScrolling)
        {
            eventHandlerResult |= HandlePointerUp(event, mousePosition, prevPosition, MouseButtonState::LEFT, MouseButtonState::NONE, /* allowClick */ true);
        }

        m_primaryTouchPointerId = -1;
        m_touchIsScrolling = false;
        m_touchScrollTarget = WeakHandle<UIObject>();
        m_touchDownRayTestResults.Clear();
        m_touchScrollRemainder = Vec2f::Zero();

        break;
    }
    case EventType::MOUSESCROLL:
    {
        const Vec2f mousePosition = ToLogicalCoords(Vec2f(inputManager->GetMousePosition()));
        const Vec2f mouseScreen = mousePosition / Vec2f(m_surfaceSize);

        Vec2i wheel = event.GetMouseWheel();

        Array<Handle<UIObject>> rayTestResults;

        if (TestRay(mouseScreen, rayTestResults))
        {
            UIObject* firstHit = nullptr;

            for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
            {
                const Handle<UIObject>& uiObject = *it;

                // if (firstHit) {
                //     // We don't want to check the current object if it's not a child of the first hit object,
                //     // since it would be behind the first hit object.
                //     if (!firstHit->IsOrHasParent(uiObject)) {
                //         continue;
                //     }
                // } else {
                //     firstHit = uiObject;
                // }

                UIEventHandlerResult currentResult = OnScroll.Fire(uiObject, MouseEvent {
                    .baseEvent = &event,
                    .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                    .absolutePos = Vec2f(mousePosition),
                    .absolutePrevPos = Vec2f(previousMousePosition),
                    .mouseButtons = inputManager->GetButtonStates(),
                    .wheel = wheel
                });

                eventHandlerResult |= currentResult;

                if (eventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                {
                    break;
                }
            }
        }

        break;
    }
    case EventType::KEYDOWN:
    {
        const KeyCode keyCode = event.GetKeyCode();

        if (!m_keyedDownObjects.HasIndex(uint32(keyCode)))
        {
            m_keyedDownObjects.Emplace(uint32(keyCode));
        }

        auto& keyedDown = m_keyedDownObjects[uint32(keyCode)];

        Handle<UIObject> uiObject = m_focusedObject.Lock();

        while (uiObject != nullptr)
        {
            auto it = keyedDown.FindAs(uiObject);

            if (it == keyedDown.End() || ShouldTriggerKeyDownEvent(it->second))
            {
                // newly pressed, or we've been holding long enough that we should trigger another event
                UIObjectKeyState& keyState = keyedDown[uiObject];

                ++keyState.count;
                keyState.heldTime = 0.0f;

                UIEventHandlerResult currentResult = OnKeyDown.Fire(uiObject, KeyboardEvent {
                    .baseEvent = &event,
                    .inputManager = inputManager,
                    .keyCode = keyCode
                });

                eventHandlerResult |= currentResult;

                if (eventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                {
                    break;
                }
            }

            if (UIObject* parent = uiObject->GetParentUIObject())
            {
                uiObject = MakeStrongRef(parent);
            }
            else
            {
                uiObject = nullptr;
            }
        }

        break;
    }
    case EventType::KEYUP:
    {
        const KeyCode keyCode = event.GetKeyCode();

        if (!m_keyedDownObjects.HasIndex(uint32(keyCode)))
        {
            break;
        }

        for (auto& it : m_keyedDownObjects[uint32(keyCode)])
        {
            WeakHandle<UIObject>& weakUiObject = it.first;

            if (Handle<UIObject> uiObject = weakUiObject.Lock(); uiObject.IsValid())
            {
                OnKeyUp.Fire(uiObject, KeyboardEvent {
                    .baseEvent = &event,
                    .inputManager = inputManager,
                    .keyCode = keyCode
                });
            }
        }

        m_keyedDownObjects.EraseAt(uint32(keyCode));

        break;
    }
    case EventType::TEXT_INPUT:
    {
        if (Handle<UIObject> uiObject = m_focusedObject.Lock(); uiObject.IsValid())
        {
            eventHandlerResult |= OnTextInput.Fire(uiObject, event.GetTextInput());
        }

        break;
    }
    default:
        break;
    }

    return eventHandlerResult;
}

UIEventHandlerResult UIStage::HandlePointerDown(const Event& event, Vec2f mousePosition, Vec2f previousMousePosition, EnumFlags<MouseButtonState> buttons)
{
    UIEventHandlerResult eventHandlerResult = UIEventHandlerResult::OK;

    const Vec2f mouseScreen = mousePosition / Vec2f(m_surfaceSize);

    // project a ray into the scene and test if it hits any objects
    Array<Handle<UIObject>> rayTestResults;

    if (TestRay(mouseScreen, rayTestResults))
    {
        UIObject* firstHit = nullptr;

        for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
        {
            const Handle<UIObject>& uiObject = *it;

            auto mouseButtonPressedStatesIt = m_objectMouseStates.FindAs(uiObject);

            if (mouseButtonPressedStatesIt != m_objectMouseStates.End())
            {
                if ((mouseButtonPressedStatesIt->second.mouseButtons & buttons) == buttons)
                {
                    // already holding buttons, go to next
                    continue;
                }

                mouseButtonPressedStatesIt->second.mouseButtons |= buttons;
            }
            else
            {
                if (!uiObject->AcceptsFocus() || !uiObject->IsEnabled())
                {
                    continue;
                }

                if (!firstHit)
                {
                    firstHit = uiObject.Get();

                    uiObject->Focus();
                }

                mouseButtonPressedStatesIt = m_objectMouseStates.Set(uiObject, { buttons, 0.0f }).first;
            }

            mouseButtonPressedStatesIt->second.originalMousePosition = mouseScreen;
            mouseButtonPressedStatesIt->second.heldTime = 0.0f; // reset held time

            if (buttons & MouseButtonState::LEFT)
            {
                uiObject->SetFocusState(uiObject->GetFocusState() | UIObjectFocusState::PRESSED);
            }

            const UIEventHandlerResult onMouseDownResult = OnMouseDown.Fire(uiObject, MouseEvent {
                .baseEvent = &event,
                .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                .absolutePos = Vec2f(mousePosition),
                .absolutePrevPos = Vec2f(previousMousePosition),
                .mouseButtons = mouseButtonPressedStatesIt->second.mouseButtons
            });

            eventHandlerResult |= onMouseDownResult;

            if (eventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
            {
                break;
            }
        }
    }

    return eventHandlerResult;
}

UIEventHandlerResult UIStage::HandlePointerMove(const Event& event, Vec2f mousePosition, Vec2f previousMousePosition, EnumFlags<MouseButtonState> buttons)
{
    // check intersects with objects on mouse movement.
    // for any objects that had mouse held on them,
    // if the mouse is on them, signal mouse movement

    // project a ray into the scene and test if it hits any objects

    UIEventHandlerResult eventHandlerResult = UIEventHandlerResult::OK;

    const Vec2f mouseScreen = mousePosition / Vec2f(m_surfaceSize);
    const Vec2f invSurfaceSize = Vec2f::One() / Vec2f(m_surfaceSize);

    if (buttons != MouseButtonState::NONE)
    { // mouse drag event
        UIEventHandlerResult mouseDragEventHandlerResult = UIEventHandlerResult::OK;

        for (const Pair<WeakHandle<UIObject>, UIObjectMouseState>& it : m_objectMouseStates)
        {
            if (it.second.mouseButtons & buttons)
            {
                // signal mouse drag
                if (Handle<UIObject> uiObject = it.first.Lock(); uiObject.IsValid())
                {
                    MouseEvent mouseEvent {
                        .baseEvent = &event,
                        .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                        .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                        .absolutePos = mousePosition,
                        .absolutePrevPos = previousMousePosition,
                        .mouseButtons = buttons
                    };

                    if (MathUtil::Abs(it.second.originalMousePosition - mouseScreen).LengthSquared() < invSurfaceSize.LengthSquared())
                    {
                        // If the mouse position hasn't changed significantly, don't trigger a drag event
                        continue;
                    }

                    UIEventHandlerResult currentResult = OnMouseDrag.Fire(uiObject, mouseEvent);

                    mouseDragEventHandlerResult |= currentResult;

                    if (mouseDragEventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                    {
                        break;
                    }
                }
            }
        }
    }

    Array<Handle<UIObject>> rayTestResults;

    if (TestRay(mouseScreen, rayTestResults))
    {
        UIObject* firstHit = nullptr;

        UIEventHandlerResult mouseHoverEventHandlerResult = UIEventHandlerResult::OK;
        UIEventHandlerResult mouseMoveEventHandlerResult = UIEventHandlerResult::OK;

        for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
        {
            if (const Handle<UIObject>& uiObject = *it)
            {
                if (firstHit != nullptr)
                {
                    // We don't want to check the current object if it's not a child of the first hit object,
                    // since it would be behind the first hit object.
                    if (!firstHit->IsOrHasParent(uiObject))
                    {
                        continue;
                    }
                }
                else
                {
                    firstHit = uiObject;
                }

                if (m_hoveredUiObjects.Contains(uiObject))
                {
                    // Already hovered, trigger mouse move event instead
                    UIEventHandlerResult currentResult = OnMouseMove.Fire(uiObject, MouseEvent {
                        .baseEvent = &event,
                        .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                        .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                        .absolutePos = Vec2f(mousePosition),
                        .absolutePrevPos = Vec2f(previousMousePosition),
                        .mouseButtons = buttons
                    });

                    mouseMoveEventHandlerResult |= currentResult;

                    if (mouseMoveEventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                    {
                        break;
                    }
                }
            }
        }

        firstHit = nullptr;

        for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
        {
            if (const Handle<UIObject>& uiObject = *it)
            {
                if (firstHit != nullptr)
                {
                    // We don't want to check the current object if it's not a child of the first hit object,
                    // since it would be behind the first hit object.
                    if (!firstHit->IsOrHasParent(uiObject))
                    {
                        continue;
                    }
                }
                else
                {
                    firstHit = uiObject;
                }

                if (!uiObject->AcceptsFocus() || !uiObject->IsEnabled())
                {
                    continue;
                }

                if (!m_hoveredUiObjects.Insert(uiObject).second)
                {
                    continue;
                }

                uiObject->SetFocusState(uiObject->GetFocusState() | UIObjectFocusState::HOVER);

                UIEventHandlerResult currentResult = OnMouseHover.Fire(uiObject, MouseEvent {
                    .baseEvent = &event,
                    .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                    .absolutePos = Vec2f(mousePosition),
                    .absolutePrevPos = Vec2f(previousMousePosition),
                    .mouseButtons = buttons
                });

                mouseHoverEventHandlerResult |= currentResult;

                if (mouseHoverEventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                {
                    break;
                }
            }
        }
    }

    for (auto it = m_hoveredUiObjects.Begin(); it != m_hoveredUiObjects.End();)
    {
        const bool isInBounds = mouseScreen.x >= 0.0f && mouseScreen.y >= 0.0f
            && mouseScreen.x < 1.0f && mouseScreen.y < 1.0f;

        const auto rayTestResultsIt = isInBounds ? rayTestResults.FindAs(*it) : rayTestResults.End();

        if (rayTestResultsIt == rayTestResults.End())
        {
            if (Handle<UIObject> uiObject = it->Lock(); uiObject.IsValid())
            {
                auto mouseStatesIt = m_objectMouseStates.Find(uiObject);
                if (mouseStatesIt != m_objectMouseStates.End())
                {
                    EnumFlags<MouseButtonState>& stateMouseButtons = mouseStatesIt->second.mouseButtons;

                    if (stateMouseButtons != MouseButtonState::NONE) // no overlap; skip
                    {
                        // trigger mouse up
                        uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::PRESSED);

                        UIEventHandlerResult currentResult = OnMouseUp.Fire(uiObject.Get(), MouseEvent {
                            .baseEvent = &event,
                            .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                            .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                            .absolutePos = mousePosition,
                            .absolutePrevPos = previousMousePosition,
                            .mouseButtons = stateMouseButtons
                        });

                        eventHandlerResult |= currentResult;
                        mouseStatesIt = m_objectMouseStates.Erase(mouseStatesIt);
                    }
                }

                uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::HOVER);

                OnMouseLeave.Fire(uiObject, MouseEvent {
                    .baseEvent = &event,
                    .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                    .absolutePos = mousePosition,
                    .absolutePrevPos = previousMousePosition,
                    .mouseButtons = buttons
                });
            }

            it = m_hoveredUiObjects.Erase(it);
        }
        else
        {
            ++it;
        }
    }

    return eventHandlerResult;
}

UIEventHandlerResult UIStage::HandlePointerUp(const Event& event, Vec2f mousePosition, Vec2f previousMousePosition, EnumFlags<MouseButtonState> buttons, EnumFlags<MouseButtonState> otherHeldButtons, bool allowClick)
{
    UIEventHandlerResult eventHandlerResult = UIEventHandlerResult::OK;

    const Vec2f mouseScreen = mousePosition / Vec2f(m_surfaceSize);

    Array<Handle<UIObject>> rayTestResults;
    TestRay(mouseScreen, rayTestResults);

    // Check LMB/RMB clicking if only one bit (mouse button) was pressed.
    if (allowClick && ByteUtil::BitCount(otherHeldButtons | buttons) == 1)
    {
        const auto checkClickEvent = [&](MouseButtonState mouseButtonToCheck, ScriptableDelegate<UIEventHandlerResult, const MouseEvent&>* delegatePtr = nullptr)
        {
            if (buttons != mouseButtonToCheck)
            {
                return;
            }

            for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
            {
                const Handle<UIObject>& uiObject = *it;

                auto stateIt = m_objectMouseStates.Find(uiObject);

                if (stateIt == m_objectMouseStates.End() || !(stateIt->second.mouseButtons & mouseButtonToCheck))
                {
                    continue;
                }

                const EnumFlags<MouseButtonState> currentState = stateIt->second.mouseButtons;

                // check if we should trigger a click event
                if (uiObject->IsEnabled())
                {
                    if (delegatePtr != nullptr)
                    {
                        const UIEventHandlerResult result = delegatePtr->Fire(uiObject.Get(), MouseEvent {
                            .baseEvent = &event,
                            .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                            .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                            .absolutePos = Vec2f(mousePosition),
                            .absolutePrevPos = Vec2f(previousMousePosition),
                            .mouseButtons = buttons
                        });

                        eventHandlerResult |= result;

                        if (result & UIEventHandlerResult::ERR)
                        {
                            HYP_LOG(UI, Error, "OnClick returned error: {}", result.GetMessage().GetOr("<No message>"));

                            break;
                        }

                        if (result & UIEventHandlerResult::STOP_BUBBLING)
                        {
                            break;
                        }
                    }
                }
            }
        };

        checkClickEvent(MouseButtonState::LEFT, &UIObject::OnClick);
        checkClickEvent(MouseButtonState::RIGHT, &UIObject::OnRightClick);
    }

    for (auto it = m_objectMouseStates.Begin(); it != m_objectMouseStates.End();)
    {
        EnumFlags<MouseButtonState>& stateMouseButtons = it->second.mouseButtons;

        if ((stateMouseButtons & buttons) == MouseButtonState::NONE) // no overlap; skip
        {
            ++it;

            continue;
        }

        // trigger mouse up
        if (Handle<UIObject> uiObject = it->first.Lock(); uiObject.IsValid())
        {
            // No longer pressed if left mouse btn was released
            if ((buttons & MouseButtonState::LEFT) && (stateMouseButtons & MouseButtonState::LEFT))
            {
                uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::PRESSED);
            }

            UIEventHandlerResult currentResult = OnMouseUp.Fire(uiObject.Get(), MouseEvent {
                .baseEvent = &event,
                .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                .absolutePos = Vec2f(mousePosition),
                .absolutePrevPos = Vec2f(previousMousePosition),
                .mouseButtons = stateMouseButtons & buttons
            });

            eventHandlerResult |= currentResult;

            stateMouseButtons &= (~buttons);
        }
        else
        {
            stateMouseButtons = MouseButtonState::NONE;
        }

        if (stateMouseButtons == MouseButtonState::NONE) // now empty after update; remove from the map
        {
            it = m_objectMouseStates.Erase(it);

            continue;
        }

        ++it;
    }

    return eventHandlerResult;
}

bool UIStage::Remove(const Entity* entity)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (!m_scene.IsValid())
    {
        return false;
    }

    if (!GetNode())
    {
        return false;
    }

    return GetNode()->RemoveChild(entity, /* moveToDetached */ false);
}

#pragma endregion UIStage

} // namespace Hyperion
