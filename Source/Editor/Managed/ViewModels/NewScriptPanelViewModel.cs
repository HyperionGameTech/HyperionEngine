using System;
using System.IO;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public readonly record struct ScriptLanguageEntry(
        string DisplayName,
        string LanguageArg,
        string Extension);

    public class NewScriptPanelViewModel : EditorPanelViewModel
    {
        // Invoked with (name, languageArg); called with empty strings on cancel.
        private readonly Action<string, string> _onCompleted;

        private string _scriptName = DefaultScriptName;
        private ScriptLanguageEntry _selectedLanguage;

        private static readonly ScriptLanguageEntry StrataLanguage = new("Strata", "strata", ".strata");
        private static readonly ScriptLanguageEntry CSharpLanguage = new("C#", "csharp", ".cs");

        public bool IsStrataAvailable { get; } = EngineManager.IsStrataEnabled;

        public ScriptLanguageEntry[] Languages { get; }

        private const string DefaultScriptName = "NewScript";

        private bool _nameEdited;

        public string ScriptName
        {
            get => _scriptName;
            set
            {
                if (SetProperty(ref _scriptName, value))
                {
                    _nameEdited = true;
                    CheckNameTaken();
                }
            }
        }

        private bool _isNameTaken;
        public bool IsNameTaken
        {
            get => _isNameTaken;
            private set
            {
                if (SetProperty(ref _isNameTaken, value))
                {
                    (ConfirmCommand as RelayCommand)?.RaiseCanExecuteChanged();
                }
            }
        }

        public ScriptLanguageEntry SelectedLanguage
        {
            get => _selectedLanguage;
            set
            {
                if (SetProperty(ref _selectedLanguage, value))
                {
                    // Keep the radio buttons in sync with the selection.
                    OnPropertyChanged(nameof(IsStrataSelected));
                    OnPropertyChanged(nameof(IsCSharpSelected));
                }
            }
        }

        // Radio buttons bind to these; each setter drives SelectedLanguage.
        public bool IsStrataSelected
        {
            get => _selectedLanguage.LanguageArg == "strata";
            set { if (value && IsStrataAvailable) SelectedLanguage = StrataLanguage; }
        }

        public bool IsCSharpSelected
        {
            get => _selectedLanguage.LanguageArg == "csharp";
            set { if (value) SelectedLanguage = CSharpLanguage; }
        }

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public NewScriptPanelViewModel(Action<string, string> onCompleted)
            : base("New Script")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            Languages = IsStrataAvailable
                ? [StrataLanguage, CSharpLanguage]
                : [CSharpLanguage];

            _selectedLanguage = Languages[0];

            ConfirmCommand = new RelayCommand(OnConfirm, () => !IsNameTaken);
            CancelCommand = new RelayCommand(OnCancel);

            _ = EngineManager.PostToSimThread(() =>
            {
                string uniqueName = AssetManager.Instance.AssetRegistry
                    .GetUniqueAssetName(AssetBucket.Scripts.Value, DefaultScriptName).ToString();

                Dispatcher.UIThread.Post(() =>
                {
                    if (!_nameEdited)
                    {
                        _scriptName = uniqueName;
                        OnPropertyChanged(nameof(ScriptName));
                    }

                    CheckNameTaken();
                });
            });
        }

        private void CheckNameTaken()
        {
            string name = (_scriptName ?? string.Empty).Trim();

            if (name.Length == 0)
            {
                IsNameTaken = false;
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                AssetRegistry registry = AssetManager.Instance.AssetRegistry;
                string scriptsDir = Path.Combine(registry.GetRootPath(), "Scripts");

                bool taken = registry.GetUniqueAssetName(AssetBucket.Scripts.Value, name).ToString() != name
                    || File.Exists(Path.Combine(scriptsDir, name + CSharpLanguage.Extension))
                    || File.Exists(Path.Combine(scriptsDir, name + StrataLanguage.Extension));

                Dispatcher.UIThread.Post(() =>
                {
                    if ((_scriptName ?? string.Empty).Trim() == name)
                    {
                        IsNameTaken = taken;
                    }
                });
            });
        }

        private void OnConfirm()
        {
            // escape the string to be a valid identifier
            string name = (ScriptName ?? string.Empty).Trim();

            if (string.IsNullOrWhiteSpace(name) || name.Contains(' ') || IsNameTaken)
            {
                return;
            }

            _onCompleted(name, SelectedLanguage.LanguageArg);
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(string.Empty, string.Empty);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
