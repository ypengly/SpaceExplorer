using UnrealBuildTool;

public class SpaceExplorer : ModuleRules
{
    public SpaceExplorer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "ProceduralMeshComponent"
        });
        string[] Dirs = { "Core", "Player", "Vitals", "Inventory", "SaveSystem", "Galaxy", "World", "Ship", "UI",
                          "Data", "Progression", "Creatures", "NPC" };
        PublicIncludePaths.Add(ModuleDirectory);
        foreach (string D in Dirs) { PublicIncludePaths.Add(ModuleDirectory + "/" + D); }
    }
}
