using System;
using System.Collections.Generic;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;
using Hyperion.Editor.Views;
using Hyperion.Editor.Views.Inspector;

namespace Hyperion.Editor.ViewModels
{
    [EditorPlugin("AnimToolkit")]
    public sealed class AnimationEditorWiring : IEditorPlugin
    {
        public static AnimationEditorWiring? Instance { get; private set; }

        public AnimationTimelineViewModel Timeline { get; } = new AnimationTimelineViewModel();

        public AnimationGraphDocumentViewModel GraphDocument { get; } = new AnimationGraphDocumentViewModel();

        public void Register(EditorPluginRegistry registry)
        {
            Instance = this;

            registry.RegisterView<AnimationTimelineViewModel, AnimationTimelineBar>();
            registry.RegisterView<AnimationGraphDocumentViewModel, AnimationGraphDocumentView>();
            registry.RegisterView<SkeletonChoicePropertyViewModel, SkeletonChoicePropertyEditor>();
            registry.RegisterView<AnimationPreviewViewModel, AnimationPreviewEditor>();

            registry.AddEditModeBar(Timeline);
            registry.AddDocument(GraphDocument);

            registry.OnEditModeChanged(OnEditModeChanged);
            registry.OnSelectionChanged(OnSelectionChanged);
            registry.OnSimulationChanged(isSimulating =>
            {
                if (!isSimulating)
                {
                    GraphDocument.DetachDebug();
                }
            });

            registry.AddToolMenuItem("Show Bones", new RelayCommand(ToggleShowBones), () => GetExtension()?.IsShowBonesEnabled() ?? false);

            registry.AddNodeMenuItem("Edit Animation",
                node => GetExtension()?.CanEditAnimation(node) ?? false,
                node =>
                {
                    if (node is Entity entity)
                    {
                        RequestEditAnimation(state => state.EnterEntity(entity));
                    }
                });

            registry.RegisterPropertyEditor("Bone", (classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, isNameType) =>
                isNameType ? new SkeletonChoicePropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, SkeletonChoiceKind.Bone) : null);

            registry.RegisterPropertyEditor("Socket", (classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, isNameType) =>
                isNameType ? new SkeletonChoicePropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, SkeletonChoiceKind.Socket) : null);

            registry.RegisterPropertyEditor("AnimationClip", (classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, _) =>
                new SkeletonChoicePropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, SkeletonChoiceKind.AnimationClip));

            registry.AddComponentInspectorItem("AnimationComponent", entity => new AnimationPreviewViewModel(entity));

            registry.AddInspectorWarning(CheckAnimationHasSkeleton);
            registry.AddInspectorWarning(CheckClipMatchesSkeleton);

            registry.RegisterAssetIcon("Animation", "AssetAnimation");
            registry.RegisterAssetIcon("AnimationTrack", "AssetAnimationTrack");
            registry.RegisterAssetIcon("EditorBoneNode", "AssetSkeleton");

            registry.AddEntityDropAssetClass("Animation");
            registry.AddEntityDropAssetClass("AnimationGraph");

            registry.AddCreatableAssetClass("AnimationGraph");
        }

        /// <summary>Opens animation edit mode once any current edit mode is left; <paramref name="enter"/> runs on the sim thread.</summary>
        public static void RequestEditAnimation(Func<EditorAnimationEditState, bool> enter)
        {
            MainWindowViewModel.Instance?.RequestEnterEditMode("animation", () =>
            {
                EditorAnimationEditState? animationEditState = EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditState();

                return animationEditState != null && enter(animationEditState);
            });
        }

        public void OpenGraphDebugger(Entity entity)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (MainWindowViewModel.Instance?.IsSimulating ?? false)
            {
                GraphDocument.AttachDebug(entity);
            }
        }

        private static AnimationEditorExtension? GetExtension()
        {
            return EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditorExtension();
        }

        private static void ToggleShowBones()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                if (GetExtension() is AnimationEditorExtension animationExtension)
                {
                    animationExtension.SetShowBonesEnabled(!animationExtension.IsShowBonesEnabled());
                }
            });
        }

        // the timeline shows for skeleton and clip editing and stays under the graph; the graph document only for graphs
        private void OnEditModeChanged(EditorIsolatedEditState? activeState)
        {
            EditorAnimationEditState? animationEditState = activeState as EditorAnimationEditState;

            bool isEditingAnimation = animationEditState != null;
            bool isEditingGraph = isEditingAnimation && animationEditState!.IsEditingGraph();

            Dispatcher.UIThread.Post(() =>
            {
                Timeline.SetActive(isEditingAnimation);
                GraphDocument.SetActive(isEditingGraph);
            });
        }

        private void OnSelectionChanged(IReadOnlyList<Node> selectedNodes)
        {
            if (selectedNodes.Count == 1 && selectedNodes[0] is Entity entity)
            {
                GraphDocument.FollowSelection(entity);
            }
        }

        private static Skeleton? GetSkeleton(Entity entity)
        {
            return entity.TryGetComponent(out MeshComponent meshComponent) ? meshComponent.Skeleton : null;
        }

        private static string? CheckAnimationHasSkeleton(Entity entity)
        {
            if (!entity.TryGetComponent(out AnimationComponent _) || GetSkeleton(entity) != null)
            {
                return null;
            }

            return "AnimationComponent will be non-functional because no skeleton is associated with the MeshComponent.";
        }

        private static string? CheckClipMatchesSkeleton(Entity entity)
        {
            if (!entity.TryGetComponent(out AnimationComponent animationComponent))
            {
                return null;
            }

            Animation? clip = animationComponent.Animation;
            Skeleton? skeleton = GetSkeleton(entity);

            if (clip == null || skeleton == null || skeleton.CountBoundTracks(clip) > 0)
            {
                return null;
            }

            return $"Clip {clip.Name} doesn't drive any bone of skeleton {skeleton.Name}.";
        }
    }
}
