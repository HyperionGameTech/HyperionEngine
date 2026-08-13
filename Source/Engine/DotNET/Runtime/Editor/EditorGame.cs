using System;

namespace Hyperion
{
    [ClassBinding(Name = "EditorGame")]
    public class EditorGame : Game
    {
        public EditorGame()
        {
        }

        public EditorSubsystem? EditorSubsystem => this.GetEditorSubsystem(); // extension method
    }
}
