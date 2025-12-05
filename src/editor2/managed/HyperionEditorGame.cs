using Hyperion;
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
        }

        public override void OnLaunch()
        {
            Logger.Log(LogType.Debug, "HyperionEditorGame Launched");

            World.WorldFlags |= WorldFlags.EditorWorld;

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
            AssetBatch ab = new AssetBatch();
            ab.Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
            ab.Add("test_model", "models/sponza/sponza.obj");
            _assetBatchTask = ab.Load();
        }

        public override void OnUpdate(float deltaTime)
        {
            // Logger.Log(LogType.Debug, "HyperionEditorGame Update called with deltaTime: " + deltaTime);

            if (_assetBatchTask != null && _assetBatchTask.IsCompleted)
            {
                AssetMap assetMap = _assetBatchTask.Result;

                var zombieAsset = assetMap["zombie"];

                if (zombieAsset != null && zombieAsset.IsValid)
                {
                    Logger.Log(LogType.Debug, "Zombie asset loaded successfully.");

                    Assert.Throw(zombieAsset.Value != null);

                    _editorSubsystem!.GetActiveScene().RootNode.AddChild((Node)zombieAsset.Value);
                }
                else
                {
                    Logger.Log(LogType.Error, "Failed to load zombie asset.");
                }

                var testModelAsset = assetMap["test_model"];

                if (testModelAsset != null && testModelAsset.IsValid)
                {
                    Logger.Log(LogType.Debug, "Test model asset loaded successfully.");
                    Assert.Throw(testModelAsset.Value != null);

                    Node n = _editorSubsystem!.GetActiveScene().RootNode.AddChild((Node)testModelAsset.Value);
                    n.SetLocalScale(new Vec3f(0.05f));
                }
                else
                {
                    Logger.Log(LogType.Error, "Failed to load test model asset.");
                }

                _assetBatchTask = null; // Prevent repeated checks
            }
        }

        private void OnFocusedNodeChanged(Node newNode, Node prevNode, bool shouldSelectInOutline)
        {
            Logger.Log(LogType.Debug, "Focused node changed from " + (prevNode != null ? prevNode.Name.ToString() : "null") +
                       " to " + (newNode != null ? newNode.Name.ToString() : "null") +
                       ", shouldSelectInOutline: " + shouldSelectInOutline);
        }

        private void HandleProjectOpened(EditorProject project)
        {
            WeakReference weakThis = new WeakReference(this);

            _onActionStackStateChanged?.Remove();
            _onActionStackStateChanged = project.ActionStack.GetOnStateChangeDelegate()
                .Bind((EditorActionStackState newState) =>
                {
                    if (weakThis.Target is HyperionEditorGame editorGame)
                    {
                        editorGame.UpdateUndo();
                        editorGame.UpdateRedo();
                    }
                });

            Logger.Log(LogType.Info, "Project opened: " + (project != null ? project.Name.ToString() : "null"));

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
                        Logger.Log(LogType.Debug, "Child node '" + child.Name.ToString() + "' added" + "' (isDirect: " + isDirect + ")");
                    });

                editorGame._onChildRemoved = node.GetOnChildRemovedDelegate()
                    .Bind((Node child, bool isDirect) =>
                    {
                        Logger.Log(LogType.Debug, "Child node '" + child.Name.ToString() + "' removed" + "' (isDirect: " + isDirect + ")");
                    });
            };

            var addRootNodeChangedHandler = (HyperionEditorGame editorGame, Scene scene) =>
            {
                editorGame._onRootNodeChanged?.Remove();
                editorGame._onRootNodeChanged = scene.GetOnRootNodeChangedDelegate()
                    .Bind((Node newRoot, Node oldRoot) =>
                    {
                        if (weakThis.Target is HyperionEditorGame editorGame)
                        {
                            Logger.Log(LogType.Info, "Root node changed from " + (oldRoot != null ? oldRoot.Name.ToString() : "null") +
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
                .Bind((Scene scene) =>
                {
                    if (weakThis.Target is HyperionEditorGame editorGame)
                    {
                        Logger.Log(LogType.Info, "Active scene changed to: " + (scene != null ? scene.Name.ToString() : "null"));

                        addRootNodeChangedHandler(editorGame, scene);
                    }
                });
        }

        private void HandleProjectClosing(EditorProject project)
        {
            Logger.Log(LogType.Info, "Project closing: " + (project != null ? project.Name.ToString() : "null"));

            _onActionStackStateChanged?.Remove();
            _onActionStackStateChanged = null;
        }

        private void UpdateUndo()
        {
            Logger.Log(LogType.Debug, "UpdateUndo called");

            // @TODO: Model after EditorMain.cpp
        }

        private void UpdateRedo()
        {
            Logger.Log(LogType.Debug, "UpdateRedo called");


            // @TODO: Model after EditorMain.cpp
        }
    }
}