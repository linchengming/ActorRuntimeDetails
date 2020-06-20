// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARDUEFeatures.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FActorRuntimeDetailsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	void RegisterTabSpawner();
	
	/** This function will be bound to Command. */
	void PluginButtonClicked();
	
private:

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	TSharedRef<class SWidget> CreateActorRuntimeDetails(const FName TabIdentifier);
	TSharedRef<class SDockTab> SpawnActorRuntimeDetailsTab(const FSpawnTabArgs& Args);

	/** Called when actors are selected or unselected */
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh = false);
	
private:
	FDelegateHandle LevelEditorTabManagerChangedHandle;
	
	TSharedPtr<class FUICommandList> PluginCommands;
	
	/** List of all actor details panels to update when selection changes */
	TArray< TWeakPtr<class SActorRuntimeDetails> > AllActorDetailPanels;
};
