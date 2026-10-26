using System;
using System.Collections.Generic;
using System.Linq;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public static class InspectorWarnings
    {
        private const int MaxSkinnedBones = 128;

        private static readonly Func<Entity, string?>[] s_entityChecks =
        {
            CheckDynamicBodyIsNotStatic,
            CheckSkeletonBoneCount
        };

        public static List<string> Collect(IReadOnlyList<Entity> entities)
        {
            List<string> warnings = new();

            foreach (Func<Entity, string?> check in s_entityChecks.Concat(EditorPluginService.Instance.InspectorWarnings))
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

        private static string? CheckSkeletonBoneCount(Entity entity)
        {
            Skeleton? skeleton = GetSkeleton(entity);

            if (skeleton == null)
            {
                return null;
            }

            int numBones = skeleton.GetBoneNames().Length;

            return numBones > MaxSkinnedBones ? $"Skeleton {skeleton.Name} has {numBones} bones which is over the limit ({MaxSkinnedBones})" : null;
        }
    }
}
