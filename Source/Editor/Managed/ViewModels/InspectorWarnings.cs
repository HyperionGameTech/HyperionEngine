using System;
using System.Collections.Generic;

namespace Hyperion.Editor.ViewModels
{
    public static class InspectorWarnings
    {
        private static readonly Func<Entity, string?>[] s_entityChecks =
        {
            CheckDynamicBodyIsNotStatic
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
    }
}
