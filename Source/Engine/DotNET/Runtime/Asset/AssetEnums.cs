namespace Hyperion
{
    [ClassBinding(Name = "AddAssetConflictMode")]
    public enum AddAssetConflictMode : byte
    {
        FailOnConflict = 0,
        GenerateNewName,
        ReplaceExisting,
        Default
    };
}