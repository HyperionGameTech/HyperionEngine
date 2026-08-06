using System;
using System.Collections.Generic;
using System.Windows.Input;
using Hyperion.Editor.Commands;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Services
{
    public sealed class PanelService
    {
        public static PanelService Instance { get; } = new PanelService();

        private readonly List<EditorPanelViewModel> _stack = new();

        public EditorPanelViewModel? ActivePanel => _stack.Count > 0 ? _stack[^1] : null;

        public bool CanGoBack => _stack.Count > 1;

        public event EventHandler? ActivePanelChanged;

        public ICommand CloseCommand { get; }
        public ICommand BackCommand { get; }

        private PanelService()
        {
            CloseCommand = new RelayCommand(ClosePanel);
            BackCommand = new RelayCommand(GoBack, () => CanGoBack);
        }

        public void OpenPanel(EditorPanelViewModel panel)
        {
            if (panel == null)
                throw new ArgumentNullException(nameof(panel));

            if (ReferenceEquals(ActivePanel, panel))
                return;

            _stack.Add(panel);

            NotifyChanged();
        }

        public void GoBack()
        {
            if (_stack.Count <= 1)
                return;

            PopTop();
        }

        public void ClosePanel()
        {
            if (_stack.Count == 0)
                return;

            for (int i = _stack.Count - 1; i >= 0; i--)
            {
                _stack[i].OnClosed?.Invoke();
            }

            _stack.Clear();

            NotifyChanged();
        }

        /// <summary>
        /// Removes a specific panel from the stack, wherever it sits, e.g. when its
        /// source data becomes invalid. No-op if the panel isn't on the stack.
        /// </summary>
        public void RemovePanel(EditorPanelViewModel panel)
        {
            int index = _stack.IndexOf(panel);
            if (index < 0)
                return;

            _stack.RemoveAt(index);
            panel.OnClosed?.Invoke();

            NotifyChanged();
        }

        private void PopTop()
        {
            EditorPanelViewModel top = _stack[^1];
            _stack.RemoveAt(_stack.Count - 1);
            top.OnClosed?.Invoke();

            NotifyChanged();
        }

        private void NotifyChanged()
        {
            ActivePanelChanged?.Invoke(this, EventArgs.Empty);
            (BackCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }
    }
}
