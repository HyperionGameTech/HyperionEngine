using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    /// <summary>
    /// Tracks how the inspector routes property edits for the selected entity:
    /// override mode ON applies to the World's active layer's override set only;
    /// OFF applies to the base set (keeping the active layer's existing overrides in sync).
    /// Read by property view models when committing changes.
    /// </summary>
    public static class LayerOverrideEditContext
    {
        public static Entity? CurrentEntity { get; set; }

        public static bool OverrideModeActive { get; set; }

        /// <summary>Name of the World's active layer, or null when unknown.</summary>
        public static string? ActiveLayerName { get; set; }

        public static void Reset()
        {
            CurrentEntity = null;
            ActiveLayerName = null;
        }
    }
}
