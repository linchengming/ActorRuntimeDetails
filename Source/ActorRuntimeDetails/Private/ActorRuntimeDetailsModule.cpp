// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ActorRuntimeDetailsModule.h"
#include "ActorRuntimeDetailsStyle.h"
#include "ActorRuntimeDetailsCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"
#include "EditorStyle.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "IDocumentation.h"
#include "TutorialMetaData.h"
#include "SActorRuntimeDetails.h"
#include "Engine/Selection.h"

#if UE_4_24_OR_LATER
#include "Widgets/SWidget.h"
#include "Widgets/Docking/SDockTab.h"
#else
#include "Widgets/Docking/SDockTab.h"
#endif

static const FName ActorRuntimeDetailsTabName("ActorRuntimeDetails");

#define LOCTEXT_NAMESPACE "FActorRuntimeDetailsModule"

void FActorRuntimeDetailsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FActorRuntimeDetailsStyle::Initialize();
	FActorRuntimeDetailsStyle::ReloadTextures();

	FActorRuntimeDetailsCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FActorRuntimeDetailsCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FActorRuntimeDetailsModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
	
	if (LevelEditorTabManager)
	{
		RegisterTabSpawner();
	}
	else
	{
		LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddRaw(this, &FActorRuntimeDetailsModule::RegisterTabSpawner);
	}
	
	LevelEditorModule.OnActorSelectionChanged().AddRaw(this, &FActorRuntimeDetailsModule::OnActorSelectionChanged);
}

void FActorRuntimeDetailsModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.OnTabManagerChanged().Remove(LevelEditorTabManagerChangedHandle);
	}

	FActorRuntimeDetailsStyle::Shutdown();

	FActorRuntimeDetailsCommands::Unregister();
}

void FActorRuntimeDetailsModule::RegisterTabSpawner()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (LevelEditorTabManager)
	{
		const FText DetailsTooltip = NSLOCTEXT("LevelEditorTabs", "LevelEditorSelectionDetailsTooltip", "Open a Runtime Details tab. Use this to view and edit properties of the selected object(s).");
		const FSlateIcon DetailsIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details");

		LevelEditorTabManager->RegisterTabSpawner("LevelEditorRuntimeSelectionDetails", FOnSpawnTab::CreateRaw(this, &FActorRuntimeDetailsModule::SpawnActorRuntimeDetailsTab))
			.SetDisplayName(NSLOCTEXT("LevelEditorTabs", "LevelEditorRuntimeSelectionDetails", "Runtime Details 1"))
			.SetTooltipText(DetailsTooltip)
			.SetGroup(MenuStructure.GetLevelEditorDetailsCategory())
			.SetIcon(DetailsIcon);
	}
}

void FActorRuntimeDetailsModule::PluginButtonClicked()
{
}

void FActorRuntimeDetailsModule::AddMenuExtension(FMenuBuilder& Builder)
{
}

TSharedRef<SWidget> FActorRuntimeDetailsModule::CreateActorRuntimeDetails(const FName TabIdentifier)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

	TSharedPtr<FUICommandList> LevelEditorCommands;
	TSharedRef<SActorRuntimeDetails> ActorRuntimeDetails = SNew(SActorRuntimeDetails, TabIdentifier, LevelEditorCommands, LevelEditorTabManager);

	// Immediately update it (otherwise it will appear empty)
	{
		TArray<UObject*> SelectedActors;
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = static_cast<AActor*>(*It);
			checkSlow(Actor->IsA(AActor::StaticClass()));

			if (!Actor->IsPendingKill())
			{
				SelectedActors.Add(Actor);
			}
		}

		const bool bForceRefresh = true;
		ActorRuntimeDetails->SetObjects(SelectedActors, bForceRefresh);
	}

	AllActorDetailPanels.Add(ActorRuntimeDetails);
	return ActorRuntimeDetails;
}

TSharedRef<SDockTab> FActorRuntimeDetailsModule::SpawnActorRuntimeDetailsTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SActorRuntimeDetails> ActorRuntimeDetails = StaticCastSharedRef<SActorRuntimeDetails>(CreateActorRuntimeDetails("LevelEditorRuntimeSelectionDetails"));

	const FText Label = NSLOCTEXT("LevelEditor", "RuntimeDetailsTabTitle", "Runtime Details 1");

	TSharedRef<SDockTab> DocTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(Label)
		.ToolTip(IDocumentation::Get()->CreateToolTip(Label, nullptr, "Shared/LevelEditor", "DetailsTab"))
		[
			SNew(SBox)
			.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("ActorRuntimeDetails"), TEXT("LevelEditorSelectionDetails")))
			[
				ActorRuntimeDetails
			]
		];

	return DocTab;
}

void FActorRuntimeDetailsModule::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	for (auto It = AllActorDetailPanels.CreateIterator(); It; ++It)
	{
		TSharedPtr<SActorRuntimeDetails> ActorRuntimeDetails = It->Pin();
		if (ActorRuntimeDetails.IsValid())
		{
			ActorRuntimeDetails->SetObjects(NewSelection, bForceRefresh);
		}
		else
		{
			// remove stray entries here
		}
	}
}

void FActorRuntimeDetailsModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FActorRuntimeDetailsCommands::Get().PluginAction);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FActorRuntimeDetailsModule, ActorRuntimeDetails)