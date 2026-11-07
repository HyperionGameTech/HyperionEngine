using System;

namespace Hyperion
{
    [ClassBinding(Name = "TerrainSculptMode")]
    public enum TerrainSculptMode : byte
    {
        Raise = 0,
        Lower = 1,
        PaintSplat = 2,
        PaintGroundCover = 3,
        MarkProcedural = 4,
        Flatten = 5,
        Smooth = 6,
        Ramp = 7
    }

    [ClassBinding(Name = "EditorTerrainState")]
    public class EditorTerrainState : ObjectBase
    {
        public bool IsEnabled => this.IsEnabled();

        public bool IsSculptActive => this.IsSculptActive();

        public bool IsPaintActive => this.IsPaintActive();

        public bool IsGroundCoverPaintActive => this.IsGroundCoverPaintActive();

        public float Radius => this.GetRadius();

        public float Strength => this.GetStrength();

        public TerrainSculptMode Mode => this.GetMode();

        public int PaintLayer => this.GetPaintLayer();

        public bool EraseMode => this.GetEraseMode();

        public Prefab? ActiveGroundCoverPrefab => this.GetActiveGroundCoverPrefab();
    }
}
