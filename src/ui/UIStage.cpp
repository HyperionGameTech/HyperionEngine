/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <UIPch.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <ui/camera/UICameraController.hpp>

#include <asset/Assets.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/World.hpp>
#include <scene/EntityManager.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <ui/font/FontAtlas.hpp>

#include <rendering/Texture.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>

#include <input/InputManager.hpp>

#include <engine/EngineDriver.hpp>

#include <UIStage.generated.inl>

namespace hyperion {

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

HYP_DECLARE_LOG_CHANNEL(UI);

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
    : UIStage(ThreadId::Current())
{
}

UIStage::UIStage(ThreadId ownerThreadId)
    : UIObject(ownerThreadId),
      m_updateManager(this),
      m_surfaceSize { 1000, 1000 }
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
        const auto& controller = controllers[i];

        if (controller->IsA<UICameraController>())
        {
            currentController = ObjCast<UICameraController>(controller);
            controllerIndex = i;
            break;
        }
    }

    if (currentController)
    {
        m_camera->RemoveCameraController(currentController);
    }

    Handle<UICameraController> newController = CreateObject<UICameraController>(
        0.0f, -float(m_surfaceSize.x),
        0.0f, float(m_surfaceSize.y),
        float(MinDepth), float(MaxDepth));

    m_camera->AddCameraController(newController, controllerIndex);
}

void UIStage::SetSurfaceSize(Vec2i surfaceSize)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_surfaceSize = surfaceSize;

    if (m_camera.IsValid())
    {
        m_camera->SetDimensions(surfaceSize);

        UpdateCameraControllerStack();
    }

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
    HYP_SCOPE;

    Handle<Scene> newScene = scene;

    if (!newScene.IsValid())
    {
        const ThreadId ownerThreadId = m_scene.IsValid() ? m_scene->GetOwnerThreadId() : ThreadId::Current();

        newScene = CreateObject<Scene>(NAME_FMT("UIStage_{}_Scene", GetName()), ownerThreadId, SceneFlags::FOREGROUND | SceneFlags::UI);
        newScene->SetRoot(CreateObject<Entity>());
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

    // If no World is set for the scene, use default world
    if (m_scene && !m_scene->GetWorld())
    {
        g_engineDriver->GetDefaultWorld()->AddScene(m_scene);
    }

    InitObject(m_scene);
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

    m_camera = CreateObject<Camera>();
    m_camera->SetName(NAME_FMT("{}_UIStage_Camera", GetName()));
    m_camera->AddCameraController(CreateObject<UICameraController>(
        0.0f, -float(m_surfaceSize.x),
        0.0f, float(m_surfaceSize.y),
        float(MinDepth), float(MaxDepth)));

    InitObject(m_camera);

    const auto updateSurfaceSize = [this](ApplicationWindow* window)
    {
        m_onWindowResizedHandler.Reset();

        if (window == nullptr)
        {
            return;
        }

        const Vec2i size = window->GetSize();

        m_surfaceSize = Vec2i(size);

        if (m_camera.IsValid())
        {
            m_camera->SetDimensions(m_surfaceSize);

            UpdateCameraControllerStack();
        }

        m_onWindowResizedHandler = window->OnWindowSizeChanged.BindThreaded(
            [this](Vec2i newSize)
            {
                SetSurfaceSize(newSize);
            },
            g_gameThread);
    };

    updateSurfaceSize(g_appContext->GetMainWindow());
    m_onCurrentWindowChangedHandler = g_appContext->OnCurrentWindowChanged
                                          .BindThreaded(updateSurfaceSize, g_gameThread);

    if (!m_defaultFontAtlas)
    {
        auto fontAtlasAsset = g_assetManager->Load<FontAtlas>("fonts/default.json");

        if (fontAtlasAsset.HasValue())
        {
            m_defaultFontAtlas = fontAtlasAsset->Result();
        }
        else
        {
            HYP_LOG(UI, Error, "Failed to load default font atlas! Error was: {}", fontAtlasAsset.GetError().GetMessage());
        }
    }

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
            hit.userData = uiObject;

            rayTestResults.AddHit(hit);
        }
    }

    outObjects.Reserve(rayTestResults.Size());

    for (const RayHit& hit : rayTestResults)
    {
        if (Handle<UIObject> uiObject = static_cast<const UIObject*>(hit.userData)->HandleFromThis(); uiObject.IsValid())
        {
            outObjects.PushBack(std::move(uiObject));
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

UIEventHandlerResult UIStage::OnInputEvent(const SystemEvent& event)
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

    const Vec2i previousMousePosition = inputManager->GetPreviousMousePosition();

    switch (event.GetType())
    {
    case SystemEvent::WINDOW_FOCUS_LOST:
    {
        const Vec2i mousePosition = inputManager->GetMousePosition();

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

                        UIEventHandlerResult currentResult = uiObject->OnMouseUp(MouseEvent {
                            .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                            .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                            .absolutePos = Vec2f(mousePosition),
                            .absolutePrevPos = Vec2f(previousMousePosition),
                            .mouseButtons = stateMouseButtons
                        });

                        eventHandlerResult |= currentResult;
                        mouseStatesIt = m_objectMouseStates.Erase(mouseStatesIt);
                    }
                }

                uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::HOVER);

                uiObject->OnMouseLeave(MouseEvent {
                    .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                    .absolutePos = Vec2f(mousePosition),
                    .absolutePrevPos = Vec2f(previousMousePosition),
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
                    uiObject->OnKeyUp(KeyboardEvent {
                        .inputManager = inputManager,
                        .keyCode = keyCode
                    });
                }
            }
        }
        
        m_keyedDownObjects.Clear();

        break;
    }
    case SystemEvent::MOUSEMOTION:
    {
        // check intersects with objects on mouse movement.
        // for any objects that had mouse held on them,
        // if the mouse is on them, signal mouse movement

        // project a ray into the scene and test if it hits any objects

        const EnumFlags<MouseButtonState> mouseButtons = inputManager->GetButtonStates();

        const Vec2i mousePosition = event.GetMousePosition();
        const Vec2f mouseScreen = Vec2f(mousePosition) / Vec2f(m_surfaceSize);
        const Vec2f invSurfaceSize = Vec2f(1.0f) / Vec2f(m_surfaceSize);

        if (mouseButtons != MouseButtonState::NONE)
        { // mouse drag event
            UIEventHandlerResult mouseDragEventHandlerResult = UIEventHandlerResult::OK;

            for (const Pair<WeakHandle<UIObject>, UIObjectMouseState>& it : m_objectMouseStates)
            {
                if (it.second.mouseButtons & mouseButtons)
                {
                    // signal mouse drag
                    if (Handle<UIObject> uiObject = it.first.Lock(); uiObject.IsValid())
                    {
                        MouseEvent mouseEvent {
                            .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                            .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                            .absolutePos = Vec2f(mousePosition),
                            .absolutePrevPos = Vec2f(previousMousePosition),
                            .mouseButtons = mouseButtons
                        };

                        if (MathUtil::Abs(it.second.originalMousePosition - mouseScreen).LengthSquared() < invSurfaceSize.LengthSquared())
                        {
                            // If the mouse position hasn't changed significantly, don't trigger a drag event
                            continue;
                        }

                        UIEventHandlerResult currentResult = uiObject->OnMouseDrag(mouseEvent);

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
                        UIEventHandlerResult currentResult = uiObject->OnMouseMove(MouseEvent {
                            .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                            .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                            .absolutePos = Vec2f(mousePosition),
                            .absolutePrevPos = Vec2f(previousMousePosition),
                            .mouseButtons = mouseButtons
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

                    UIEventHandlerResult currentResult = uiObject->OnMouseHover(MouseEvent {
                        .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                        .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                        .absolutePos = Vec2f(mousePosition),
                        .absolutePrevPos = Vec2f(previousMousePosition),
                        .mouseButtons = mouseButtons
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

                            UIEventHandlerResult currentResult = uiObject->OnMouseUp(MouseEvent {
                                .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                                .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                                .absolutePos = Vec2f(mousePosition),
                                .absolutePrevPos = Vec2f(previousMousePosition),
                                .mouseButtons = stateMouseButtons
                            });

                            eventHandlerResult |= currentResult;
                            mouseStatesIt = m_objectMouseStates.Erase(mouseStatesIt);
                        }
                    }

                    uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::HOVER);

                    uiObject->OnMouseLeave(MouseEvent {
                        .relativePos = uiObject->TransformScreenCoordsToRelative(mousePosition),
                        .relativePrevPos = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                        .absolutePos = Vec2f(mousePosition),
                        .absolutePrevPos = Vec2f(previousMousePosition),
                        .mouseButtons = inputManager->GetButtonStates()
                    });
                }

                it = m_hoveredUiObjects.Erase(it);
            }
            else
            {
                ++it;
            }
        }

        break;
    }
    case SystemEvent::MOUSEBUTTON_DOWN:
    {
        const Vec2i mousePosition = inputManager->GetMousePosition();
        const Vec2f mouseScreen = Vec2f(mousePosition) / Vec2f(m_surfaceSize);
        const Vec2f invSurfaceSize = Vec2f(1.0f) / Vec2f(m_surfaceSize);

        // project a ray into the scene and test if it hits any objects
        RayHit hit;

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
                    if ((mouseButtonPressedStatesIt->second.mouseButtons & event.GetMouseButtons()) == event.GetMouseButtons())
                    {
                        // already holding buttons, go to next
                        continue;
                    }

                    mouseButtonPressedStatesIt->second.mouseButtons |= event.GetMouseButtons();
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

                    mouseButtonPressedStatesIt = m_objectMouseStates.Set(uiObject, { event.GetMouseButtons(), 0.0f }).first;
                }

                mouseButtonPressedStatesIt->second.originalMousePosition = mouseScreen;
                mouseButtonPressedStatesIt->second.heldTime = 0.0f; // reset held time

                if (event.GetMouseButtons() & MouseButtonState::LEFT)
                {
                    uiObject->SetFocusState(uiObject->GetFocusState() | UIObjectFocusState::PRESSED);
                }

                const UIEventHandlerResult onMouseDownResult = uiObject->OnMouseDown(MouseEvent {
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

        break;
    }
    case SystemEvent::MOUSEBUTTON_UP:
    {
        const Vec2i mousePosition = inputManager->GetMousePosition();
        const Vec2f mouseScreen = Vec2f(mousePosition) / Vec2f(m_surfaceSize);
        const Vec2f invSurfaceSize = Vec2f(1.0f) / Vec2f(m_surfaceSize);

        Array<Handle<UIObject>> rayTestResults;
        TestRay(mouseScreen, rayTestResults);

        const EnumFlags<MouseButtonState> buttons = event.GetMouseButtons();

        typedef ScriptableDelegate<UIEventHandlerResult, const MouseEvent&> UIObject::* ClickDelegateMember;
        const auto checkClickEvent = [&](MouseButtonState mouseButtonToCheck, ClickDelegateMember delegateMember = nullptr)
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
                    if (delegateMember != nullptr)
                    {
                        const UIEventHandlerResult result = (uiObject->*delegateMember)(MouseEvent {
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

                UIEventHandlerResult currentResult = uiObject->OnMouseUp(MouseEvent {
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

        break;
    }
    case SystemEvent::MOUSESCROLL:
    {
        const Vec2i mousePosition = inputManager->GetMousePosition();
        const Vec2f mouseScreen = Vec2f(mousePosition) / Vec2f(m_surfaceSize);

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

                UIEventHandlerResult currentResult = uiObject->OnScroll(MouseEvent {
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
    case SystemEvent::KEYDOWN:
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

                UIEventHandlerResult currentResult = uiObject->OnKeyDown(KeyboardEvent {
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
    case SystemEvent::KEYUP:
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
                uiObject->OnKeyUp(KeyboardEvent {
                    .inputManager = inputManager,
                    .keyCode = keyCode
                });
            }
        }

        m_keyedDownObjects.EraseAt(uint32(keyCode));

        break;
    }
    default:
        break;
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

    return GetNode()->RemoveChild(entity);
}

#pragma endregion UIStage

} // namespace hyperion
