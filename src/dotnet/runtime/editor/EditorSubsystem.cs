using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="EditorSubsystem")]
    public class EditorSubsystem : Subsystem
    {
        public EditorSubsystem()
        {
        }

        public EditorProject CurrentProject
        {
            get
            {
                return this.GetCurrentProject();
            }
        }
    }
}