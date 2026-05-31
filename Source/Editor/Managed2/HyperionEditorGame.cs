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
        private DelegateHandler? _onActionStackStateChanged;
        private DelegateHandler? _onFocusedNodeChanged;
        private DelegateHandler? _onRootNodeChanged;
        private DelegateHandler? _onChildAdded;
        private DelegateHandler? _onChildRemoved;
        private DelegateHandler? _onActiveSceneChanged;

        public EditorSubsystem? EditorSubsystem => _editorSubsystem;

        public HyperionEditorGame()
        {
            World = new World { Name = new Name("EditorWorld") };
            World.WorldFlags |= WorldFlags.EditorWorld;
            World.SetIsTransient(true); // Editor world should not be saved or loaded from disk.
        }

        protected override void OnLaunch()
        {
            Logger.Log(LogLevel.Debug, "HyperionEditorGame Launched");

            this.SetToEditMode();

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

            // tmp debug
            AssetBatch ab = new();
            ab.Add("test_model", "Models/SponzaGltf/Sponza.gltf"); //"Models/testbed/testbed.obj");
            ab.Add("guy", "models/ZombieGuy/guy.mesh.xml");
            _assetBatchTask = ab.Load();
        }

        protected override void OnUpdate(float deltaTime)
        {
            if (_assetBatchTask != null && _assetBatchTask.IsCompleted)
            {
                AssetMap assetMap = _assetBatchTask.Result;

                var guy = assetMap["guy"];

                if (guy != null && guy.IsValid)
                {
                    Assert.Throw(guy.Value != null);

                    Node n = _editorSubsystem!.GetActiveScene().RootNode.AddChild((Node)guy.Value);
                    n.LocalScale = new Vec3f(0.5f);
                }

                var testModelAsset = assetMap["test_model"];

                if (testModelAsset != null && testModelAsset.IsValid)
                {
                    Logger.Log(LogLevel.Debug, "Test model asset loaded successfully.");
                    Assert.Throw(testModelAsset.Value != null);

                    Node n = _editorSubsystem!.GetActiveScene().RootNode.AddChild((Node)testModelAsset.Value);
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
            Debug.Assert(project.GameInstance != null && project.GameInstance.AssetRegistry != null);

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

            _onActionStackStateChanged?.Remove();
            _onActionStackStateChanged = null;
        }
    }
}
