using UnrealBuildTool;
using System.Collections.Generic;

public class SpaceExplorerEditorTarget : TargetRules
{
    public SpaceExplorerEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("SpaceExplorer");
    }
}
