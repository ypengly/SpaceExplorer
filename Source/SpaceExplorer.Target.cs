using UnrealBuildTool;
using System.Collections.Generic;

public class SpaceExplorerTarget : TargetRules
{
    public SpaceExplorerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("SpaceExplorer");
    }
}
