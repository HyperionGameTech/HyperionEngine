using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class Vec2fViewModel : VectorPropertyViewModelBase<Vec2f>
    {
        public Vec2fViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly, 2,
                  (v, i) => i == 0 ? v.x : v.y,
                  (v, i, val) => i switch
                  {
                      0 => new Vec2f(val, v.y),
                      1 => new Vec2f(v.x, val),
                      _ => v
                  })
        {
        }
    }
}