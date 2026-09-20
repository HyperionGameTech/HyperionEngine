using System;
using System.Collections.ObjectModel;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class SkySettingsViewModel : ViewModelBase
    {
        internal readonly World World;
        internal readonly DynamicSkySystem SkySystem;

        public uint WorldId { get; }
        public uint SkySystemId { get; }

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        public SkySettingsViewModel(World world, uint worldId, DynamicSkySystem skySystem, uint skySystemId)
        {
            World = world ?? throw new ArgumentNullException(nameof(world));
            SkySystem = skySystem ?? throw new ArgumentNullException(nameof(skySystem));
            WorldId = worldId;
            SkySystemId = skySystemId;

            BuildPropertyViewModels();
        }

        private void BuildPropertyViewModels()
        {
            foreach (Property property in SkySystem.Class.Properties)
            {
                InspectorPropertyViewModelBase viewModel;

                try
                {
                    viewModel = InspectorViewModelFactory.Create(
                        SkySystem,
                        property,
                        isReadOnly: false,
                        postWriteCallback: MarkWorldDirty);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Debug, $"WorldSettings: skipping sky property '{property.Name}': {ex.Message}");
                    continue;
                }

                Properties.Add(viewModel);
            }
        }

        // plain field edits don't dirty the world asset, so saving would otherwise skip them
        private void MarkWorldDirty()
        {
            if (!World.IsValid)
            {
                return;
            }

            try
            {
                World.MarkDirty();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"WorldSettings: failed to mark world dirty after sky edit: {ex.Message}");
            }
        }
    }
}
