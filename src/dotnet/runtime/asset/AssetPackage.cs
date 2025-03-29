using System;
using System.IO;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion
{   
    [HypClassBinding(Name="AssetPackage")]
    public class AssetPackage : HypObject
    {
        private static LogChannel logChannel = LogChannel.ByName("Assset");

        public AssetPackage()
        {
        }

        public Name GetUniqueAssetName(Name baseAssetName)
        {
            string baseAssetNameString = baseAssetName.ToString();

            string baseDirectory = this.GetAbsolutePath();
            
            string[] directories = Array.Empty<string>();

            try
            {
                directories = System.IO.Directory.GetDirectories(baseDirectory);
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogType.Error, "Failed to get files: " + e.Message);
            }

            for (int i = 1; i < int.MaxValue; i++)
            {
                string assetNameString = baseAssetNameString + i;

                bool exists = false;

                foreach (string directory in directories)
                {
                    // get basename of dir without extension
                    string basename = System.IO.Path.GetFileNameWithoutExtension(directory);

                    if (basename.Equals(assetNameString, StringComparison.OrdinalIgnoreCase))
                    {
                        exists = true;

                        break;
                    }
                }

                if (!exists)
                    return new Name(assetNameString);
            }

            return new Name(baseAssetNameString);
        }
    }
}