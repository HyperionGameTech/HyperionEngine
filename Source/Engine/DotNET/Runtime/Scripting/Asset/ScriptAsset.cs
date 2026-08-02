namespace Hyperion
{
    [ClassBinding(Name = "ScriptAsset")]
    public class ScriptAsset : AssetObject
    {
        public ScriptAsset()
        {
        }

        public ScriptDesc ScriptDesc
        {
            get => this.GetScriptDesc();
        }
    }
}