// LiveLinkFaceCSVWriter.Build.cs

using UnrealBuildTool;
using System.Collections.Generic;

public class LiveLinkFaceCSVWriter : ModuleRules
{
    public LiveLinkFaceCSVWriter(ReadOnlyTargetRules Target) : base(Target)
    {
        // Use explicit or shared PCHs for faster compile times
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Expose our Public headers to other modules
        PublicIncludePaths.AddRange(
            new string[] {
                "LiveLinkFaceCSVWriter/Public"
            }
        );

        // Our own Private headers
        PrivateIncludePaths.AddRange(
            new string[] {
                "LiveLinkFaceCSVWriter/Private"
            }
        );

        // These are the modules we depend on at runtime
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",              // Core engine types & utilities
                "CoreUObject",       // UObject system
                "Engine",            // FPaths, FFileHelper, IFileManager
                "LiveLink",          // FLiveLinkClientReference, FLiveLinkClient
                "LiveLinkInterface"  // ILiveLinkClient, FLiveLinkSubjectFrameData, roles
            }
        );

        // No extra Editor or Slate deps needed for pure runtime CSV writing
        PrivateDependencyModuleNames.AddRange(
            new string[] { }
        );

        // No dynamically loaded modules
        DynamicallyLoadedModuleNames.AddRange(
            new string[] { }
        );
    }
}
