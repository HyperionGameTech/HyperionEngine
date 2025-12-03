using Hyperion;
using System.Threading.Tasks;

namespace Hyperion.Editor
{
    [ClassBinding(IsDynamic = true)]
    public class HyperionEditorGame : Game
    {
        private EditorSubsystem? m_editorSubsystem;
        private Task<AssetMap>? m_assetBatchTask;

        private DelegateHandler? m_onProjectOpened;
        private DelegateHandler? m_onProjectClosing;
        private DelegateHandler? m_onActionStackStateChanged;
        private DelegateHandler? m_onFocusedNodeChanged;
        private DelegateHandler? m_onRootNodeChanged;
        private DelegateHandler? m_onChildAdded;
        private DelegateHandler? m_onChildRemoved;
        private DelegateHandler? m_onActiveSceneChanged;

        public HyperionEditorGame()
        {
        }

        public override void OnLaunch()
        {
            Logger.Log(LogType.Debug, "HyperionEditorGame Launched");

            m_editorSubsystem = new EditorSubsystem();
            World.AddSubsystem(m_editorSubsystem);

            m_onFocusedNodeChanged = m_editorSubsystem.GetOnFocusedNodeChangedDelegate()
                .Bind(OnFocusedNodeChanged);

            m_onProjectOpened = m_editorSubsystem.GetOnProjectOpenedDelegate()
                .Bind(HandleProjectOpened);

            m_onProjectClosing = m_editorSubsystem.GetOnProjectClosingDelegate()
                .Bind(HandleProjectClosing);

            EditorProject? project = m_editorSubsystem.CurrentProject;

            if (project != null)
            {
                HandleProjectOpened(project);
            }

            // tmp debug
            AssetBatch ab = new AssetBatch();
            ab.Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
            ab.Add("test_model", "models/sponza/sponza.obj");
            m_assetBatchTask = ab.Load();
        }

        public override void OnUpdate(float deltaTime)
        {
            Logger.Log(LogType.Debug, "HyperionEditorGame Update called with deltaTime: " + deltaTime);

            if (m_assetBatchTask != null && m_assetBatchTask.IsCompleted)
            {
                AssetMap assetMap = m_assetBatchTask.Result;

                var zombieAsset = assetMap["zombie"];

                if (zombieAsset != null && zombieAsset.IsValid)
                {
                    Logger.Log(LogType.Debug, "Zombie asset loaded successfully.");

                    Assert.Throw(zombieAsset.Value != null);

                    m_editorSubsystem!.GetActiveScene().RootNode.AddChild((Node)zombieAsset.Value);
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

                    Node n = m_editorSubsystem!.GetActiveScene().RootNode.AddChild((Node)testModelAsset.Value);
                    n.SetLocalScale(new Vec3f(0.1f));
                }
                else
                {
                    Logger.Log(LogType.Error, "Failed to load test model asset.");
                }

                m_assetBatchTask = null; // Prevent repeated checks
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
            m_onActionStackStateChanged?.Remove();

            m_onActionStackStateChanged = project.ActionStack.GetOnStateChangeDelegate()
                .Bind((EditorActionStackState newState) =>
                {
                    UpdateUndo();
                    UpdateRedo();
                });

            Logger.Log(LogType.Info, "Project opened: " + (project != null ? project.Name.ToString() : "null"));

            Scene? activeScene = m_editorSubsystem!.GetActiveScene();

            WeakReference weakThis = new WeakReference(this);

            var setChildAddRemovedHandlers = (HyperionEditorGame editorGame, Node? node) =>
            {
                editorGame.m_onChildAdded?.Remove();
                editorGame.m_onChildRemoved?.Remove();

                if (node == null)
                {
                    return;
                }

                editorGame.m_onChildAdded = node.GetOnChildAddedDelegate()
                    .Bind((Node child, bool isDirect) =>
                    {
                        Logger.Log(LogType.Debug, "Child node '" + child.Name.ToString() + "' added" + "' (isDirect: " + isDirect + ")");
                    });

                editorGame.m_onChildRemoved = node.GetOnChildRemovedDelegate()
                    .Bind((Node child, bool isDirect) =>
                    {
                        Logger.Log(LogType.Debug, "Child node '" + child.Name.ToString() + "' removed" + "' (isDirect: " + isDirect + ")");
                    });
            };

            var addRootNodeChangedHandler = (HyperionEditorGame editorGame, Scene scene) =>
            {
                editorGame.m_onRootNodeChanged?.Remove();
                editorGame.m_onRootNodeChanged = scene.GetOnRootNodeChangedDelegate()
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

            m_onActiveSceneChanged?.Remove();
            m_onActiveSceneChanged = m_editorSubsystem!.GetOnActiveSceneChangedDelegate()
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

            m_onActionStackStateChanged?.Remove();
            m_onActionStackStateChanged = null;
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