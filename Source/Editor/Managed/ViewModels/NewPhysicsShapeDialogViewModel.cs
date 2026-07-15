using System.Collections.ObjectModel;
using System.Linq;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ShapeTypeEntry
    {
        public string DisplayName { get; }
        public PhysicsShapeType Value { get; }

        public ShapeTypeEntry(string displayName, PhysicsShapeType value)
        {
            DisplayName = displayName;
            Value = value;
        }

        public PhysicsShape CreateInstance()
        {
            return Value switch
            {
                PhysicsShapeType.Box => new BoxPhysicsShape(),
                PhysicsShapeType.Sphere => new SpherePhysicsShape(),
                PhysicsShapeType.Capsule => new CapsulePhysicsShape(),
                PhysicsShapeType.Plane => new PlanePhysicsShape(),
                PhysicsShapeType.ConvexHull => new ConvexHullPhysicsShape(),
                _ => throw new NotImplementedException() // Not handled
            };
        }
    }

    public class NewPhysicsShapeDialogViewModel : ViewModelBase
    {
        private ShapeTypeEntry? _selectedShapeType;
        private ComponentSubObjectViewModel? _shapeSubObject;

        public ObservableCollection<ShapeTypeEntry> ShapeTypes { get; } = new();

        public ShapeTypeEntry? SelectedShapeType
        {
            get => _selectedShapeType;
            set
            {
                if (SetProperty(ref _selectedShapeType, value) && value != null)
                {
                    RebuildShapeInstance(value);
                }
            }
        }

        public ComponentSubObjectViewModel? ShapeSubObject
        {
            get => _shapeSubObject;
            private set => SetProperty(ref _shapeSubObject, value);
        }

        public PhysicsShape? CreatedShape { get; private set; }

        public NewPhysicsShapeDialogViewModel()
        {
            ShapeTypes.Add(new ShapeTypeEntry("Box", PhysicsShapeType.Box));
            ShapeTypes.Add(new ShapeTypeEntry("Sphere", PhysicsShapeType.Sphere));
            ShapeTypes.Add(new ShapeTypeEntry("Capsule", PhysicsShapeType.Capsule));
            ShapeTypes.Add(new ShapeTypeEntry("Plane", PhysicsShapeType.Plane));
            ShapeTypes.Add(new ShapeTypeEntry("Convex Hull", PhysicsShapeType.ConvexHull));

            SelectedShapeType = ShapeTypes.First();
        }

        private void RebuildShapeInstance(ShapeTypeEntry entry)
        {
            CreatedShape = entry.CreateInstance();

            ShapeSubObject = new ComponentSubObjectViewModel(entry.DisplayName, CreatedShape);
        }
    }
}
