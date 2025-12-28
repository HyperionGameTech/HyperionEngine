using System;
using System.Diagnostics;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.Commands
{
    public class SetGizmoCommand : ICommand
    {
        private EditorManipulationMode _mode;

        public SetGizmoCommand(EditorManipulationMode mode)
        {
            _mode = mode;
        }

        public bool CanExecute(object? parameter) => true; // dont check as it needs to be called on the sim thread

        public void Execute(object? parameter)
        {
            EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;
            Debug.Assert(editorSubsystem != null);

            _ = EngineManager.PostToSimThread(() => editorSubsystem.SetSelectedManipulationMode(_mode));
        }

        public event EventHandler? CanExecuteChanged;

        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}