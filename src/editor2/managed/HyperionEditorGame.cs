using Hyperion;

namespace Hyperion.Editor
{
    [ClassBinding(IsDynamic = true)]
    public class HyperionEditorGame : Game
    {
        private EditorSubsystem? m_editorSubsystem;

        public HyperionEditorGame()
        {
        }

        public override void OnLaunch()
        {
            Logger.Log(LogType.Debug, "HyperionEditorGame Launched");

            m_editorSubsystem = new EditorSubsystem();
            World.AddSubsystem(m_editorSubsystem);

            EditorProject project = m_editorSubsystem.CurrentProject;

            Scene defaultScene = new Scene();
            defaultScene.Name = new Name("DefaultScene");
            project.World.AddScene(defaultScene, /* addToStreamingLayer */ true);

            DirectionalLight sun = new DirectionalLight();
            sun.Name = new Name("Sun");
            sun.Direction = new Vec3f(-0.5f, 0.5f, -0.5f).Normalize();
            defaultScene.RootNode.AddChild(sun);

            DynamicSkySubsystem dss = new DynamicSkySubsystem();
            project.World.AddSubsystem(dss);

            Logger.Log(LogType.Debug, "Default scene added to the editor game world");
        }

        public override void OnUpdate(float deltaTime)
        {
            Logger.Log(LogType.Debug, "HyperionEditorGame Update called with deltaTime: " + deltaTime);
        }
    }
}