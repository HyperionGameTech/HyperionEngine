using System;
using System.Linq;
using System.Windows.Input;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class SaveAsTemplatePanelViewModel : EditorPanelViewModel
    {
        private readonly Action<string?> _onCompleted;

        private string _templateName;

        public string TemplateName
        {
            get => _templateName;
            set
            {
                if (SetProperty(ref _templateName, value))
                {
                    OnPropertyChanged(nameof(IsNameValid));
                    OnPropertyChanged(nameof(ReplacesExistingTemplate));
                }
            }
        }

        public bool IsNameValid => IsValidTemplateName((TemplateName ?? string.Empty).Trim());

        public bool ReplacesExistingTemplate
        {
            get
            {
                string name = (TemplateName ?? string.Empty).Trim();

                return IsValidTemplateName(name)
                    && EngineManager.EditorGame?.EditorSubsystem?.HasTemplate(new Name(name)) == true;
            }
        }

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public SaveAsTemplatePanelViewModel(string defaultName, Action<string?> onCompleted)
            : base("Save as Template")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            _templateName = SanitizeTemplateName(defaultName);

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private static bool IsValidTemplateName(string name)
        {
            return name.Length > 0 && name.All(character => char.IsAsciiLetterOrDigit(character) || character == '_' || character == '-');
        }

        private static string SanitizeTemplateName(string name)
        {
            string sanitized = new string((name ?? string.Empty)
                .Trim()
                .Select(character => char.IsAsciiLetterOrDigit(character) || character == '-' ? character : '_')
                .ToArray());

            return sanitized.Length > 0 ? sanitized : "NewTemplate";
        }

        private void OnConfirm()
        {
            string name = (TemplateName ?? string.Empty).Trim();

            if (!IsValidTemplateName(name))
            {
                return;
            }

            _onCompleted(name);
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
