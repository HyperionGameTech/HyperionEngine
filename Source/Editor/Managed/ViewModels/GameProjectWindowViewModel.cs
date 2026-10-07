using System;
using System.IO;
using System.Windows.Input;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public enum GameProjectLanguage
    {
        None,
        Native,
        Managed,
        Rust
    }

    public class GameProjectWindowViewModel : ViewModelBase
    {
        private readonly GameProjectLanguage _currentLanguage;
        private GameProjectLanguage _selectedLanguage;

        public event Action? CloseRequested;

        public GameProjectLanguage ConfirmedLanguage { get; private set; }
        public bool ConfirmedOpenInVisualStudio { get; private set; }

        public bool CanOpenInVisualStudio => Services.VisualStudioService.IsAvailable;
        public bool OpenInVisualStudio { get; set; } = true;

        public bool IsChangingLanguage => _currentLanguage != GameProjectLanguage.None;

        public string Title => IsChangingLanguage ? "Change Game Project Language" : "Generate Game Project";
        public string ConfirmText => IsChangingLanguage ? "Change" : "Generate";

        public string SourceDirectory { get; }

        public bool IsNativeSelected
        {
            get => _selectedLanguage == GameProjectLanguage.Native;
            set { if (value) Select(GameProjectLanguage.Native); }
        }

        public bool IsManagedSelected
        {
            get => _selectedLanguage == GameProjectLanguage.Managed;
            set { if (value) Select(GameProjectLanguage.Managed); }
        }

        public bool IsRustSelected
        {
            get => _selectedLanguage == GameProjectLanguage.Rust;
            set { if (value) Select(GameProjectLanguage.Rust); }
        }

        public RelayCommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public GameProjectWindowViewModel(string? projectFilePath, GameProjectLanguage currentLanguage)
        {
            _currentLanguage = currentLanguage;
            _selectedLanguage = currentLanguage == GameProjectLanguage.None ? GameProjectLanguage.Managed : currentLanguage;

            // an unsaved project has no path yet; generating prompts to save it first
            SourceDirectory = string.IsNullOrEmpty(projectFilePath)
                ? string.Empty
                : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(projectFilePath)) ?? string.Empty, "Source");

            ConfirmCommand = new RelayCommand(OnConfirm, () => _selectedLanguage != GameProjectLanguage.None && _selectedLanguage != _currentLanguage);
            CancelCommand = new RelayCommand(() => CloseRequested?.Invoke());
        }

        private void Select(GameProjectLanguage language)
        {
            if (_selectedLanguage == language)
            {
                return;
            }

            _selectedLanguage = language;

            OnPropertyChanged(nameof(IsNativeSelected));
            OnPropertyChanged(nameof(IsManagedSelected));
            OnPropertyChanged(nameof(IsRustSelected));

            ConfirmCommand.RaiseCanExecuteChanged();
        }

        private void OnConfirm()
        {
            ConfirmedLanguage = _selectedLanguage;
            ConfirmedOpenInVisualStudio = CanOpenInVisualStudio && OpenInVisualStudio && _selectedLanguage != GameProjectLanguage.Rust;

            CloseRequested?.Invoke();
        }
    }
}
