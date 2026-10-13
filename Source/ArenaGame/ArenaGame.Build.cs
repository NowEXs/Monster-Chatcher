// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArenaGame : ModuleRules
{
	public ArenaGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ArenaGame",
			"ArenaGame/Variant_Platforming",
			"ArenaGame/Variant_Platforming/Animation",
			"ArenaGame/Variant_Combat",
			"ArenaGame/Variant_Combat/AI",
			"ArenaGame/Variant_Combat/Animation",
			"ArenaGame/Variant_Combat/Gameplay",
			"ArenaGame/Variant_Combat/Interfaces",
			"ArenaGame/Variant_Combat/UI",
			"ArenaGame/Variant_SideScrolling",
			"ArenaGame/Variant_SideScrolling/AI",
			"ArenaGame/Variant_SideScrolling/Gameplay",
			"ArenaGame/Variant_SideScrolling/Interfaces",
			"ArenaGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
