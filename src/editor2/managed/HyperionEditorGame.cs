using Hyperion;
using System.Threading.Tasks;

namespace Hyperion.Editor
{
    [ClassBinding(IsDynamic = true)]
    public class HyperionEditorGame : Game
    {
        private EditorSubsystem? m_editorSubsystem;
        private Task<AssetMap>? m_assetBatchTask;

        public HyperionEditorGame()
        {
        }

        public override void OnLaunch()
        {
            Logger.Log(LogType.Debug, "HyperionEditorGame Launched");

            m_editorSubsystem = new EditorSubsystem();
            World.AddSubsystem(m_editorSubsystem);

            m_editorSubsystem.GetOnFocusedNodeChangedDelegate()
                .Bind(OnFocusedNodeChanged)
                .Detach();

            EditorProject project = m_editorSubsystem.CurrentProject;

            Scene defaultScene = new Scene();
            defaultScene.Name = new Name("DefaultScene");
            project.World.AddScene(defaultScene, /* addToStreamingLayer */ true);

            WeakReference weakThis = new WeakReference(this);
            defaultScene.GetOnRootNodeChangedDelegate()
                .Bind((Node newRoot, Node oldRoot) =>
                {
                    if (weakThis.Target is HyperionEditorGame editorGame)
                    {
                        Logger.Log(LogType.Debug, "Root node changed in scene '" + newRoot.Scene!.Name.ToString() + "' from " +
                                   (oldRoot != null ? oldRoot.Name.ToString() : "null") + " to " +
                                   (newRoot != null ? newRoot.Name.ToString() : "null"));
                    }
                })
                .Detach();

            defaultScene.RootNode.GetOnChildAddedDelegate()
                .Bind((Node child, bool isDirect) =>
                {
                    if (weakThis.Target is HyperionEditorGame editorGame)
                    {
                        Logger.Log(LogType.Debug, "Child node '" + child.Name.ToString() + "' added to parent node '" +
                                   child.Parent!.Name.ToString() + "' (isDirect: " + isDirect + ")");
                    }
                })
                .Detach();

            defaultScene.RootNode.GetOnChildRemovedDelegate()
                .Bind((Node child, bool isDirect) =>
                {
                    if (weakThis.Target is HyperionEditorGame editorGame)
                    {
                        Logger.Log(LogType.Debug, "Child node '" + child.Name.ToString() + "' removed from parent node '" +
                                   (child.Parent != null ? child.Parent.Name.ToString() : "null") + "' (isDirect: " + isDirect + ")");
                    }
                })
                .Detach();

            DirectionalLight sun = new DirectionalLight();
            sun.Name = new Name("Sun");
            sun.Direction = new Vec3f(0.5f, 0.5f, 0.5f).Normalize();
            sun.Intensity = 10.0f;
            defaultScene.RootNode.AddChild(sun);

            m_editorSubsystem.SetFocusedNode(sun, /* shouldSelectInOutline */ true);

            DynamicSkySubsystem dss = new DynamicSkySubsystem();
            project.World.AddSubsystem(dss);

            Logger.Log(LogType.Debug, "Default scene added to the editor game world");

            var assetBatch = new AssetBatch();
            assetBatch.Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
            assetBatch.Add("test_model", "models/sponza/sponza.obj");

            m_assetBatchTask = assetBatch.Load();
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
                    n.Scale(new Vec3f(0.05f));
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
    }
}