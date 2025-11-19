using UnrealBuildTool;
using System;
using System.IO;
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

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "EditorSubsystem",
            "EditorFramework",
            "LevelEditor"
        });

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
                "TimeManagement",
                "Projects"
            }
        );

        // Check if LiveLinkMultiIPhone plugin exists
        bool bHasLiveLinkMultiIPhone = false;
        string PluginDirectory = Path.Combine(ModuleDirectory, "../../../LiveLinkMultiIPhone");
        if (Directory.Exists(PluginDirectory))
        {
            PrivateDependencyModuleNames.Add("LiveLinkMultiIPhone");
            bHasLiveLinkMultiIPhone = true;
        }

        // Define preprocessor macro for conditional compilation
        PublicDefinitions.Add("WITH_LIVELINKMULTIIPHONE=" + (bHasLiveLinkMultiIPhone ? "1" : "0"));

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
        );
    }
}