using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class EntityTagItemViewModel : ViewModelBase
    {
        public EntityTag Tag { get; }
        public string Name { get; }

        public EntityTagItemViewModel(EntityTag tag)
        {
            Tag = tag;
            Name = tag.Name;
        }
    }

    public class EntityTagOptionViewModel : ViewModelBase
    {
        public EntityTag Tag { get; }
        public string Name { get; }

        public EntityTagOptionViewModel(EntityTag tag)
        {
            Tag = tag;
            Name = tag.Name;
        }
    }

    public class EntityTagsViewModel : ViewModelBase
    {
        private readonly Entity _entity;

        public ObservableCollection<EntityTagItemViewModel> Tags { get; } = new ObservableCollection<EntityTagItemViewModel>();
        public ObservableCollection<EntityTagOptionViewModel> AvailableTags { get; } = new ObservableCollection<EntityTagOptionViewModel>();

        private EntityTagOptionViewModel? _selectedAvailableTag;
        public EntityTagOptionViewModel? SelectedAvailableTag
        {
            get => _selectedAvailableTag;
            set
            {
                if (SetProperty(ref _selectedAvailableTag, value) && AddTagCommand is RelayCommand<EntityTagOptionViewModel> relayCommand)
                {
                    relayCommand.RaiseCanExecuteChanged();
                }
            }
        }

        private bool _hasTags;
        public bool HasTags
        {
            get => _hasTags;
            private set => SetProperty(ref _hasTags, value);
        }

        private bool _hasAvailableTags;
        public bool HasAvailableTags
        {
            get => _hasAvailableTags;
            private set => SetProperty(ref _hasAvailableTags, value);
        }

        public ICommand AddTagCommand { get; }
        public ICommand RemoveTagCommand { get; }

        public EntityTagsViewModel(Entity entity)
        {
            _entity = entity;

            AddTagCommand = new RelayCommand<EntityTagOptionViewModel>(
                option => _ = AddTagAsync(option),
                option => option != null);

            RemoveTagCommand = new RelayCommand<EntityTagItemViewModel>(
                item => _ = RemoveTagAsync(item));

            _ = RefreshAsync();
        }

        public async Task RefreshAsync()
        {
            if (_entity == null || !_entity.IsValid)
            {
                return;
            }

            EntityTag[] editorFriendlyTags = EntityTag.GetEditorFriendlyTags();
            var currentTags = new List<EntityTag>();

            await EngineManager.PostToSimThread(() =>
            {
                EntityManager? mgr = _entity.EntityManager;

                if (mgr == null)
                {
                    return;
                }

                foreach (EntityTag tag in editorFriendlyTags)
                {
                    if (mgr.HasTag(_entity, tag))
                    {
                        currentTags.Add(tag);
                    }
                }
            });

            Dispatcher.UIThread.Post(() =>
            {
                HashSet<ulong> currentValues = currentTags.Select(t => t.Value).ToHashSet();

                Tags.Clear();

                foreach (EntityTag tag in currentTags)
                {
                    Tags.Add(new EntityTagItemViewModel(tag));
                }

                HasTags = Tags.Count > 0;

                EntityTag? previousSelection = SelectedAvailableTag?.Tag;

                AvailableTags.Clear();

                foreach (EntityTag tag in editorFriendlyTags)
                {
                    if (!currentValues.Contains(tag.Value))
                    {
                        AvailableTags.Add(new EntityTagOptionViewModel(tag));
                    }
                }

                HasAvailableTags = AvailableTags.Count > 0;

                SelectedAvailableTag = previousSelection.HasValue
                    ? AvailableTags.FirstOrDefault(o => o.Tag.Value == previousSelection.Value.Value)
                    : null;
            });
        }

        private async Task AddTagAsync(EntityTagOptionViewModel? option)
        {
            if (option == null || _entity == null || !_entity.IsValid)
            {
                return;
            }

            EntityTag tag = option.Tag;
            Entity capturedEntity = _entity;

            await EngineManager.PostToSimThread(() =>
            {
                EntityManager? mgr = capturedEntity.EntityManager;

                if (mgr == null)
                {
                    return;
                }

                mgr.AddTag(capturedEntity, tag);

                EditorProject? project = EngineManager.CurrentProject;
                Debug.Assert(project != null, "No active project found when adding an entity tag");

                project?.ActionStack?.PushAction(new EditorAction(
                    $"Add Tag: {tag.Name}",
                    execute: (_, _) => capturedEntity.EntityManager?.AddTag(capturedEntity, tag),
                    revert: (_, _) => capturedEntity.EntityManager?.RemoveTag(capturedEntity, tag)));
            });

            await RefreshAsync();
        }

        private async Task RemoveTagAsync(EntityTagItemViewModel? item)
        {
            if (item == null || _entity == null || !_entity.IsValid)
            {
                return;
            }

            EntityTag tag = item.Tag;
            Entity capturedEntity = _entity;

            await EngineManager.PostToSimThread(() =>
            {
                EntityManager? mgr = capturedEntity.EntityManager;

                if (mgr == null)
                {
                    return;
                }

                mgr.RemoveTag(capturedEntity, tag);

                EditorProject? project = EngineManager.CurrentProject;
                Debug.Assert(project != null, "No active project found when removing an entity tag");

                project?.ActionStack?.PushAction(new EditorAction(
                    $"Remove Tag: {tag.Name}",
                    execute: (_, _) => capturedEntity.EntityManager?.RemoveTag(capturedEntity, tag),
                    revert: (_, _) => capturedEntity.EntityManager?.AddTag(capturedEntity, tag)));
            });

            await RefreshAsync();
        }
    }
}
