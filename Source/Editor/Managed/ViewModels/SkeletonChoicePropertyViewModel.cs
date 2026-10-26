using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public enum SkeletonChoiceKind
    {
        Bone,
        AnimationClip
    }

    public class SkeletonChoicePropertyViewModel : InspectorPropertyViewModelBase
    {
        public const string NoneChoice = "(none)";

        private readonly Entity _ownerEntity;
        private readonly SkeletonChoiceKind _kind;

        private string? _selectedChoice;

        public SkeletonChoicePropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, Entity ownerEntity, SkeletonChoiceKind kind)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _ownerEntity = ownerEntity;
            _kind = kind;
        }

        public override bool SupportsMultipleTargets => false;

        public ObservableCollection<string> Choices { get; } = new ObservableCollection<string>();

        public string? SelectedChoice
        {
            get => _selectedChoice;
            set
            {
                if (SetProperty(ref _selectedChoice, value) && !IsApplyingModelValue && value != null)
                {
                    CommitChoice(value);
                }
            }
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                List<string> choices = new List<string> { NoneChoice };
                string currentChoice = NoneChoice;

                try
                {
                    Skeleton? skeleton = GetSkeleton();

                    if (skeleton != null)
                    {
                        Array names = _kind == SkeletonChoiceKind.Bone ? skeleton.GetBoneNames() : skeleton.GetAnimationNames();

                        foreach (Name name in names)
                        {
                            choices.Add(name.ToString());
                        }
                    }

                    if (TryReadSharedValue(out object? rawValue))
                    {
                        string? rawName = rawValue switch
                        {
                            Name name => name.ToString(),
                            AssetObject asset => asset.Name.ToString(),
                            _ => null
                        };

                        if (!string.IsNullOrEmpty(rawName))
                        {
                            currentChoice = rawName;

                            // a clip from another skeleton, or a bone the skeleton lost, still shows as set
                            if (!choices.Contains(rawName))
                            {
                                choices.Add(rawName);
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            if (!Choices.SequenceEqual(choices))
                            {
                                Choices.Clear();

                                foreach (string choice in choices)
                                {
                                    Choices.Add(choice);
                                }
                            }

                            Value = currentChoice;
                            SelectedChoice = currentChoice;
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private Skeleton? GetSkeleton()
        {
            if (_ownerEntity.EntityManager == null || !_ownerEntity.TryGetComponent(out MeshComponent meshComponent))
            {
                return null;
            }

            return meshComponent.Skeleton;
        }

        private void CommitChoice(string choice)
        {
            string captured = choice == NoneChoice ? string.Empty : choice;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    if (_kind == SkeletonChoiceKind.Bone)
                    {
                        using BoxedValue boxedName = new BoxedValue(new Name(captured));
                        CommitPropertyChange($"Set {Label}", boxedName);

                        return;
                    }

                    Animation? clip = captured.Length > 0 ? GetSkeleton()?.FindAnimation(new Name(captured)) : null;

                    using BoxedValue boxedClip = new BoxedValue(clip);
                    CommitPropertyChange($"Set {Label}", boxedClip);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to write property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }
    }
}
