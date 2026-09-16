using System;
using System.Collections.ObjectModel;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    /// <summary>
    /// Environment section of the World Settings window: sky look, sky light, exposure and height fog.
    /// </summary>
    public class EnvironmentSettingsViewModel : ViewModelBase
    {
        private const string EnvironmentPropertyName = "EnvironmentSettings";

        internal readonly World World;

        public uint WorldId { get; }

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        public EnvironmentSettingsViewModel(World world, uint worldId)
        {
            World = world ?? throw new ArgumentNullException(nameof(world));
            WorldId = worldId;

            BuildPropertyViewModels();
        }

        private void BuildPropertyViewModels()
        {
            foreach (Property property in World.Class.Properties)
            {
                if (property.Name.ToString() != EnvironmentPropertyName)
                {
                    continue;
                }

                try
                {
                    Properties.Add(InspectorViewModelFactory.Create(
                        World,
                        property,
                        isReadOnly: false,
                        postWriteCallback: MarkWorldDirty));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"WorldSettings: failed to create environment settings: {ex.Message}");
                }

                break;
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
                Logger.Log(LogLevel.Warning, $"WorldSettings: failed to mark world dirty after environment edit: {ex.Message}");
            }
        }
    }
}
