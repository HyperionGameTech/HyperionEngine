using System;

namespace Hyperion
{
    [ClassBinding(Name = "CharacterFacingMode")]
    public enum CharacterFacingMode : uint
    {
        None = 0,
        MovementDirection = 1,
        ViewDirection = 2
    }
}
