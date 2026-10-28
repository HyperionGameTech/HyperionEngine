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
            registry.RegisterView<AnimationGraphLinkViewModel, AnimationGraphLinkEditor>();
            registry.RegisterView<AnimationGraphInspectorViewModel, AnimationGraphInspectorView>();
            registry.RegisterView<AnimationGraphParameterChoiceViewModel, AnimationGraphParameterChoiceEditor>();

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

            registry.AddNodeMenuItem("Edit Animation Graph",
                node => node is Entity entity && GetState()?.FindEntityGraph(entity) != null,
                node =>
                {
                    if (node is Entity entity)
                    {
                        OpenEntityGraph(entity);
                    }
                });

            registry.RegisterPropertyEditor("Bone", (classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, isNameType) =>
                isNameType ? new SkeletonChoicePropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, SkeletonChoiceKind.Bone) : null);

            registry.RegisterPropertyEditor("Socket", (classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, isNameType) =>
                isNameType ? new SkeletonChoicePropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, SkeletonChoiceKind.Socket) : null);

            registry.RegisterObjectPropertyEditor("GraphParameter", (target, property, isReadOnly, isNameType) =>
                isNameType ? new AnimationGraphParameterChoiceViewModel(target, property, isReadOnly) : null);

            registry.RegisterPropertyEditor("AnimationClip", (classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, _) =>
                new SkeletonChoicePropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, ownerEntity, SkeletonChoiceKind.AnimationClip));

            registry.AddComponentInspectorItem("AnimationComponent", entity => new AnimationPreviewViewModel(entity));
            registry.AddComponentInspectorItem("CharacterModelComponent", entity => new AnimationGraphLinkViewModel(entity));

            registry.AddInspectorWarning(CheckAnimationHasSkeleton);
            registry.AddInspectorWarning(CheckClipMatchesSkeleton);

            registry.RegisterAssetIcon("Animation", "AssetAnimation");
            registry.RegisterAssetIcon("AnimationTrack", "AssetAnimationTrack");
            registry.RegisterAssetIcon("AnimationGraph", "Graph");
            registry.RegisterAssetIcon("EditorBoneNode", "AssetSkeleton");

            registry.AddEntityDropAssetClass("Animation");
            registry.AddEntityDropAssetClass("AnimationGraph");

            registry.AddCreatableAssetClass("AnimationGraph");
        }

        /// <summary>Opens animation edit mode once any current edit mode is left.</summary>
        public static void RequestEditAnimation(Func<EditorAnimationEditState, bool> enter)
        {
            MainWindowViewModel.Instance?.RequestEnterEditMode("animation", () =>
            {
                EditorAnimationEditState? animationEditState = EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditState();

                return animationEditState != null && enter(animationEditState);
            });
        }

        public static void OpenEntityGraph(Entity entity)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (MainWindowViewModel.Instance?.IsSimulating ?? false)
            {
                Instance?.OpenGraphDebugger(entity);

                return;
            }

            RequestEditAnimation(state => state.EnterEntityGraph(entity));
        }

        public void OpenGraphDebugger(Entity entity)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (!(MainWindowViewModel.Instance?.IsSimulating ?? false))
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                Entity? running = GetState()?.FindRunningGraphEntity(entity);

                if (running != null)
                {
                    Dispatcher.UIThread.Post(() => GraphDocument.AttachDebug(running));
                }
            });
        }

        internal static EditorAnimationEditState? GetState()
        {
            return EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditState();
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
