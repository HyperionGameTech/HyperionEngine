using Hyperion;
using System.Diagnostics;
using System.Threading.Tasks;

namespace Hyperion.Editor
{
    [ClassBinding(IsDynamic = true)]
    public class HyperionEditorGame : Game
    {
        private EditorSubsystem? _editorSubsystem;
        private Task<AssetMap>? _assetBatchTask;

        private DelegateHandler? _onProjectOpened;
        private DelegateHandler? _onProjectClosing;

        private DelegateHandler? _onFocusedNodeChanged;
        private DelegateHandler? _onRootNodeChanged;
        private DelegateHandler? _onChildAdded;
        private DelegateHandler? _onChildRemoved;
        private DelegateHandler? _onActiveSceneChanged;

        public EditorSubsystem? EditorSubsystem => _editorSubsystem;

        public HyperionEditorGame()
        {
            PackageName = new Name("HyperionEditorGame");
        }

        protected override void OnLaunch()
        {
            Logger.Log(LogLevel.Debug, "HyperionEditorGame Launched");

            Debug.Assert(World != null);

            // get or create UISubsystem instance.
            UISubsystem? uiSubsystem = World.GetSubsystem<UISubsystem>();
            Debug.Assert(uiSubsystem != null);

            if (uiSubsystem == null)
            {
                uiSubsystem = new UISubsystem();
                World.AddSubsystem(uiSubsystem);
            }

            //uiSubsystem.AddDebugOverlay(new BaseStatsOverlay());
            uiSubsystem.AddDebugOverlay(new DeviceDetailsOverlay());
            uiSubsystem.AddDebugOverlay(new StatsOverlay());

            _editorSubsystem = new EditorSubsystem();
            World.AddSubsystem(_editorSubsystem);

            _onFocusedNodeChanged = _editorSubsystem.GetOnFocusedNodeChangedDelegate()
                .Bind(OnFocusedNodeChanged);

            _onProjectOpened = _editorSubsystem.GetOnProjectOpenedDelegate()
                .Bind(HandleProjectOpened);

            _onProjectClosing = _editorSubsystem.GetOnProjectClosingDelegate()
                .Bind(HandleProjectClosing);

            EditorProject? project = _editorSubsystem.CurrentProject;

            if (project != null)
            {
                HandleProjectOpened(project);
            }

            // // tmp debug
            // AssetBatch ab = new();
            // ab.Add("test_model", "Models/SponzaGltf/Sponza.gltf");//"Models/SanMiguel/san-miguel-low-poly.obj");//LivingRoom/living_room.obj");//// //"Models/testbed/testbed.obj");
            // ab.Add("guy", "models/ZombieGuy/guy.mesh.xml");
            // _assetBatchTask = ab.Load();
        }

        protected override World LoadWorld(Name unusedName)
        {
            var world = new World { Name = new Name("EditorWorld") };
            world.WorldFlags |= WorldFlags.EditorWorld;
            world.SetIsTransient(true); // Editor world should not be saved or loaded from disk.

            return world;
        }

        protected override void BeforeShutdown()
        {
            Logger.Log(LogLevel.Debug, "HyperionEditorGame BeforeShutdown");

            _onProjectOpened?.Remove();
            _onProjectClosing?.Remove();

            _onFocusedNodeChanged?.Remove();
            _onRootNodeChanged?.Remove();
            _onChildAdded?.Remove();
            _onChildRemoved?.Remove();
            _onActiveSceneChanged?.Remove();

            _editorSubsystem = null;
        }

        protected override void OnUpdate(float deltaTime)
        {
            if (_assetBatchTask != null && _assetBatchTask.IsCompleted)
            {
                AssetMap assetMap = _assetBatchTask.Result;

                Scene activeScene = _editorSubsystem!.GetActiveScene();

                activeScene.World!.AddSystem(new CharacterControllerSystem());

                Node rootNode = activeScene.RootNode!;

                // Ground: a static floor for the character controller to walk on.
                Entity groundEntity = new Entity();
                groundEntity.Name = new Name("Ground");
                rootNode.AddChild(groundEntity);

                BoxPhysicsShape groundShape = new BoxPhysicsShape();
                groundShape.GetProperty(new Name("Bounds")).Set(groundShape,
                    new BoxedValue(new BoundingBox(new Vec3f(-50.0f, -0.5f, -50.0f), new Vec3f(50.0f, 0.5f, 50.0f))));

                RigidBodyComponent groundRbc = new RigidBodyComponent();
                groundRbc.PhysicsMaterial = new PhysicsMaterial();
                groundRbc.Shape = new Handle<PhysicsShape>(groundShape);
                groundEntity.AddComponent(ref groundRbc);
                groundRbc.Dispose();

                // Player: character controller driven by WASD + Space.
                Entity playerEntity = new Entity();
                playerEntity.Name = new Name("Player");
                rootNode.AddChild(playerEntity);
                playerEntity.LocalTranslation = new Vec3f(0.0f, 3.0f, 0.0f);

                CapsulePhysicsShape capsuleShape = new CapsulePhysicsShape();
                capsuleShape.Radius = 0.4f;
                capsuleShape.Height = 1.0f;

                // CharacterControllerComponent cc = new CharacterControllerComponent();
                // cc.Shape = new Handle<PhysicsShape>(capsuleShape);
                // cc.MoveSpeed = 5.0f;
                // cc.ViewDirection = new Vec3f(0.0f, 0.0f, 1.0f);
                // playerEntity.AddComponent(ref cc);
                // cc.Dispose();

                // Attach guy mesh to the player so it visually follows the controller.
                var guy = assetMap["guy"];

                if (guy != null && guy.IsValid)
                {
                    Assert.Throw(guy.Value != null);

                    Node guyNode = playerEntity.AddChild(((Prefab)guy.Value).Root);
                    guyNode.LocalScale = new Vec3f(0.5f);
                }

                // Sponza test model.
                var testModelAsset = assetMap["test_model"];

                if (testModelAsset != null && testModelAsset.IsValid)
                {
                    Logger.Log(LogLevel.Debug, "Test model asset loaded successfully.");
                    Assert.Throw(testModelAsset.Value != null);

                    Node n = rootNode.AddChild(((Prefab)testModelAsset.Value).Root);
                    n.LocalScale = new Vec3f(3.0f);
                }
                else
                {
                    Logger.Log(LogLevel.Error, "Failed to load test model asset.");
                }

                _assetBatchTask = null; // Prevent repeated checks
            }
        }

        private void OnFocusedNodeChanged(Node? newNode, Node? prevNode, bool shouldSelectInOutline)
        {
            Logger.Log(LogLevel.Debug, "Focused node changed from " + (prevNode != null ? prevNode.Name.ToString() : "null") +
                       " to " + (newNode != null ? newNode.Name.ToString() : "null") +
                       ", shouldSelectInOutline: " + shouldSelectInOutline);
        }

        private void HandleProjectOpened(EditorProject project)
        {
            Debug.Assert(project.GameInstance != null && project.GameInstance.IsValid);
            Debug.Assert(project.GameInstance.AssetRegistry != null && project.GameInstance.AssetRegistry.IsValid);

            AssetRegistry = project.GameInstance.AssetRegistry;

            WeakReference weakThis = new WeakReference(this);

            Logger.Log(LogLevel.Info, "Project opened: " + (project != null ? project.Name.ToString() : "null"));

            Scene? activeScene = _editorSubsystem!.GetActiveScene();

            var setChildAddRemovedHandlers = (HyperionEditorGame editorGame, Node? node) =>
            {
                editorGame._onChildAdded?.Remove();
                editorGame._onChildRemoved?.Remove();

                if (node == null)
                {
                    return;
                }

                editorGame._onChildAdded = node.GetOnChildAddedDelegate()
                    .Bind((Node child, bool isDirect) =>
                    {
                        Logger.Log(LogLevel.Debug, "Child node '" + child.Name.ToString() + "' added" + "' (isDirect: " + isDirect + ")");
                    });

                editorGame._onChildRemoved = node.GetOnChildRemovedDelegate()
                    .Bind((Node child, bool isDirect) =>
                    {
                        Logger.Log(LogLevel.Debug, "Child node '" + child.Name.ToString() + "' removed" + "' (isDirect: " + isDirect + ")");
                    });
            };

            var addRootNodeChangedHandler = (HyperionEditorGame editorGame, Scene? scene) =>
            {
                editorGame._onRootNodeChanged?.Remove();

                if (scene == null)
                {
                    return;
                }

                editorGame._onRootNodeChanged = scene.GetOnRootNodeChangedDelegate()
                    .Bind((Node? newRoot, Node? oldRoot) =>
                    {
                        if (weakThis.Target is HyperionEditorGame editorGame)
                        {
                            Logger.Log(LogLevel.Info, "Root node changed from " + (oldRoot != null ? oldRoot.Name.ToString() : "null") +
                                    " to " + (newRoot != null ? newRoot.Name.ToString() : "null"));

                            setChildAddRemovedHandlers(editorGame, newRoot);
                        }
                    });

                setChildAddRemovedHandlers(editorGame, scene.RootNode);
            };

            if (activeScene != null)
            {
                addRootNodeChangedHandler(this, activeScene);
            }

            _onActiveSceneChanged?.Remove();
            _onActiveSceneChanged = _editorSubsystem!.GetOnActiveSceneChangedDelegate()
                .Bind((Scene? scene) =>
                {
                    if (weakThis.Target is HyperionEditorGame editorGame)
                    {
                        Logger.Log(LogLevel.Info, "Active scene changed to: " + (scene != null ? scene.Name.ToString() : "null"));

                        addRootNodeChangedHandler(editorGame, scene);
                    }
                });
        }

        private void HandleProjectClosing(EditorProject project)
        {
            Logger.Log(LogLevel.Info, "Project closing: " + (project != null ? project.Name.ToString() : "null"));

            // Unset scene-specific event handlers, otherwise we could call a method on
            // a disposed Scene.

            _onRootNodeChanged?.Remove();
            _onRootNodeChanged = null;

            _onChildAdded?.Remove();
            _onChildAdded = null;

            _onChildRemoved?.Remove();
            _onChildRemoved = null;

            _onActiveSceneChanged?.Remove();
            _onActiveSceneChanged = null;
        }
    }
}
