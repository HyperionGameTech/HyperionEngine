global using DecalId = System.UInt32;

using System;

namespace Hyperion
{
    [ClassBinding(Name = "Decal")]
    public class Decal : AssetObject
    {
        public Decal()
        {
        }

        public Material? Material => this.GetMaterial();    // Extension Method
    }

    [ClassBinding(Name = "DecalProxy")]
    public class DecalProxy : Entity
    {
        public DecalProxy()
        {
        }

        public Decal? Decal => this.GetDecal();             // Extension Method

        public uint NumDecals => this.NumDecals();          // Extension Method
    }
}
