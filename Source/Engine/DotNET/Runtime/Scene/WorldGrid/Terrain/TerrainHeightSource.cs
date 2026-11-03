namespace Hyperion
{
    [ClassBinding(Name = "TerrainHeightSource")]
    public enum TerrainHeightSource : byte
    {
        Procedural = 0,
        Flat = 1,
        Heightmap = 2
    }
}
