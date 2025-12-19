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

        public EditorProject CurrentProject => this.GetCurrentProject(); // extension method
        public EditorGizmoBase SelectedGizmo => this.GetSelectedGizmo(); // extension method
        public EditorViewport? ActiveViewport => this.GetActiveViewport(); // extension method
    }
}