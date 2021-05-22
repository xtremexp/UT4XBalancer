namespace UnrealBuildTool.Rules
{
	public class UT4XBalancer : ModuleRules
	{
		public UT4XBalancer(TargetInfo Target)
        {
			PrivateIncludePaths.Add("UT4XBalancer/Private");
			
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "UnrealTournament"
                }
			);
        }
	}
}