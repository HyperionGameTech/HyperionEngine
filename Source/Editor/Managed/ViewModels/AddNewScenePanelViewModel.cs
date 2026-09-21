using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public sealed class NewSceneResult
    {
        public string Name { get; }
        public SceneFlags Flags { get; }

        public NewSceneResult(string name, SceneFlags flags)
        {
            Name = name;
            Flags = flags;
        }
    }

    public class AddNewScenePanelViewModel : EditorPanelViewModel
    {
        private readonly Action<NewSceneResult?> _onCompleted;

        private string _sceneName = "NewScene";

        public string SceneName
        {
            get => _sceneName;
            set => SetProperty(ref _sceneName, value);
        }

        public ObservableCollection<FlagsPropertyViewModel.EnumFlagEntry> FlagEntries { get; } = new();

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public AddNewScenePanelViewModel(Action<NewSceneResult?> onCompleted)
            : base("Add New Scene")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);

            BuildFlagEntries();
        }

        private SceneFlags BuildSceneFlags()
        {
            ulong combined = 0;

            foreach (var entry in FlagEntries)
            {
                if (entry.IsSelected == true && entry.Value != null)
                {
                    combined |= Convert.ToUInt64(entry.Value);
                }
            }

            return (SceneFlags)combined;
        }

        private void OnConfirm()
        {
            _onCompleted(new NewSceneResult(SceneName, BuildSceneFlags()));
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.RemovePanel(this);
        }

        private void BuildFlagEntries()
        {
            Class? sceneFlagsClass = Class.TryGetClass(typeof(SceneFlags));

            if (sceneFlagsClass == null)
                return;

            ulong defaultVal = (ulong)SceneFlags.Default;

            foreach (StaticField staticField in sceneFlagsClass.Value.StaticFields)
            {
                ClassAttribute? attrEditor = staticField.GetAttribute("editor");

                if (attrEditor != null && attrEditor.Value.GetBool() == false)
                {
                    continue;
                }

                object? flagValue = staticField.ReadObject();

                string title = GetFlagEntryTitle(staticField);
                string description = GetFlagEntryDescription(staticField);

                var entry = new FlagsPropertyViewModel.EnumFlagEntry(title, description, flagValue, () => { });
                FlagEntries.Add(entry);

                if (flagValue != null)
                {
                    ulong flagVal = Convert.ToUInt64(flagValue);
                    entry.IsSelected = (defaultVal & flagVal) == flagVal && flagVal != 0;
                }
            }
        }

        private static string GetFlagEntryTitle(StaticField staticField)
        {
            ClassAttribute? titleAttribute = staticField.GetAttribute("title");

            return titleAttribute?.GetString() ?? staticField.Name.ToString();
        }

        private static string GetFlagEntryDescription(StaticField staticField)
        {
            ClassAttribute? descriptionAttribute = staticField.GetAttribute("description");

            return descriptionAttribute?.GetString() ?? string.Empty;
        }
    }
}
