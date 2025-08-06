using UnrealBuildTool;

public class LiveLinkFaceCSVWriter : ModuleRules
{
    public LiveLinkFaceCSVWriter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
                // ... add public include paths required here ...
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                // ... add other private include paths required here ...
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "LiveLinkInterface",
                "LiveLink",
                "TimeManagement"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "LiveLinkInterface",
                "LiveLink",
                "TimeManagement"
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
        );
    }
}