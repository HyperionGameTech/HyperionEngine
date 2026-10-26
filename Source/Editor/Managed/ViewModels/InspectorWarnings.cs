using System;
using System.Collections.Generic;

namespace Hyperion.Editor.ViewModels
{
    public static class InspectorWarnings
    {
        private const int MaxSkinnedBones = 128;

        private static readonly Func<Entity, string?>[] s_entityChecks =
        {
            CheckDynamicBodyIsNotStatic,
            CheckAnimationHasSkeleton,
            CheckClipMatchesSkeleton,
            CheckSkeletonBoneCount
        };

        public static List<string> Collect(IReadOnlyList<Entity> entities)
        {
            List<string> warnings = new();

            foreach (Func<Entity, string?> check in s_entityChecks)
            {
                string? message = null;
                int affected = 0;

                foreach (Entity entity in entities)
                {
                    if (!entity.IsValid)
                    {
                        continue;
                    }

                    string? entityMessage = check(entity);

                    if (entityMessage != null)
                    {
                        message ??= entityMessage;
                        affected++;
                    }
                }

                if (message != null)
                {
                    warnings.Add(entities.Count > 1 ? $"{message} ({affected} of {entities.Count} selected)" : message);
                }
            }

            return warnings;
        }

        private static string? CheckDynamicBodyIsNotStatic(Entity entity)
        {
            if (!entity.TryGetComponent(out RigidBodyComponent rigidBody) || rigidBody.PhysicsMaterial.Mass == 0.0f)
            {
                return null;
            }

            if (entity.IsDynamic)
            {
                return null;
            }

            return "Rigid body has a non-zero mass but the entity is static.\n"
                + "This may cause performance degredation and visual irregularities (due to caching) when/if the entity is moved.";
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

            return "Animation component has nothing to animate: the mesh has no skeleton.";
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

        private static string? CheckSkeletonBoneCount(Entity entity)
        {
            Skeleton? skeleton = GetSkeleton(entity);

            if (skeleton == null)
            {
                return null;
            }

            int numBones = skeleton.GetBoneNames().Length;

            return numBones > MaxSkinnedBones ? $"Skeleton {skeleton.Name} has {numBones} bones; only the first {MaxSkinnedBones} are skinned." : null;
        }
    }
}
