// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SActorRuntimeDetails.h"
#include "Widgets/SBoxPanel.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Engine/Selection.h"
#include "UnrealEdGlobals.h"
#include "LevelEditor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SSCSRuntimeEditor.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
//#include "LevelEditorGenericDetails.h"
#include "ScopedTransaction.h"
#include "SourceCodeNavigation.h"
#include "Widgets/Docking/SDockTab.h"

class SActorRuntimeDetailsUneditableComponentWarning : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SActorRuntimeDetailsUneditableComponentWarning)
		: _WarningText()
		, _OnHyperlinkClicked()
	{}
		
		/** The rich text to show in the warning */
		SLATE_ATTRIBUTE(FText, WarningText)

		/** Called when the hyperlink in the rich text is clicked */
		SLATE_EVENT(FSlateHyperlinkRun::FOnClick, OnHyperlinkClicked)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2)
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("Icons.Warning"))
				]
				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(2)
					[
						SNew(SRichTextBlock)
						.DecoratorStyleSet(&FEditorStyle::Get())
						.Justification(ETextJustify::Left)
						.TextStyle(FEditorStyle::Get(), "DetailsView.BPMessageTextStyle")
						.Text(InArgs._WarningText)
						.AutoWrapText(true)
						+ SRichTextBlock::HyperlinkDecorator(TEXT("HyperlinkDecorator"), InArgs._OnHyperlinkClicked)
					]
			]
		];
	}
};

void SActorRuntimeDetails::Construct(const FArguments& InArgs, const FName TabIdentifier, TSharedPtr<FUICommandList> InCommandList, TSharedPtr<FTabManager> InTabManager)
{
	bSelectionGuard = false;
	bShowingRootActorNodeSelected = false;
	bSelectedComponentRecompiled = false;

	USelection::SelectionChangedEvent.AddRaw(this, &SActorRuntimeDetails::OnEditorSelectionChanged);
	
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnComponentsEdited().AddRaw(this, &SActorRuntimeDetails::OnComponentsEditedInWorld);

	FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = true;
	DetailsViewArgs.bLockable = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ComponentsAndActorsUseNameArea;
	DetailsViewArgs.NotifyHook = this;// GUnrealEd;
	DetailsViewArgs.ViewIdentifier = TabIdentifier;
	DetailsViewArgs.bCustomNameAreaLocation = true;
	DetailsViewArgs.bCustomFilterAreaLocation = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	DetailsViewArgs.HostCommandList = InCommandList;
	DetailsViewArgs.HostTabManager = InTabManager;
	DetailsView = PropPlugin.CreateDetailView(DetailsViewArgs);

	auto IsPropertyVisible = [](const FPropertyAndParent& PropertyAndParent)
	{
		// For details views in the level editor all properties are the instanced versions
		//if(PropertyAndParent.Property.HasAllPropertyFlags(CPF_DisableEditOnInstance))
		//{
		//	return false;
		//}

		return true;
	};

	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda(IsPropertyVisible));
	DetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SActorRuntimeDetails::IsPropertyReadOnly));
	DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &SActorRuntimeDetails::IsPropertyEditingEnabled));
	DetailsView->SetOnObjectArrayChanged(FOnObjectArrayChanged::CreateSP(this, &SActorRuntimeDetails::OnDetailsViewObjectArrayChanged));


	// Set up a delegate to call to add generic details to the view
	//DetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FLevelEditorGenericDetails::MakeInstance));

	GEditor->RegisterForUndo(this);

	ComponentsBox = SNew(SBox)
		.Visibility(EVisibility::Collapsed);

	SCSRuntimeEditor = SNew(SSCSRuntimeEditor)
		.EditorMode(EComponentEditorMode::ActorInstance)
		.AllowEditing(this, &SActorRuntimeDetails::GetAllowComponentTreeEditing)
		.ActorContext(this, &SActorRuntimeDetails::GetActorContext)
		.OnSelectionUpdated(this, &SActorRuntimeDetails::OnSCSRuntimeEditorTreeViewSelectionChanged)
		.OnItemDoubleClicked(this, &SActorRuntimeDetails::OnSCSRuntimeEditorTreeViewItemDoubleClicked);
		
	ComponentsBox->SetContent(SCSRuntimeEditor.ToSharedRef());

	TextBlock = SNew(STextBlock)
		.Visibility(this, &SActorRuntimeDetails::GetPlayInGameTipVisibility)
		.Text(NSLOCTEXT("SActorRuntimeDetails", "PlayGameInEditorTip", "Play game in editor."))
		.ShadowOffset(FVector2D(1, 1));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.Padding(2.0f, 24.0f, 2.0f, 2.0f)
		.AutoHeight()
		[
			TextBlock.ToSharedRef()
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		.AutoHeight()
		[
			DetailsView->GetNameAreaWidget().ToSharedRef()
		]
		+SVerticalBox::Slot()
		[
			SAssignNew(DetailsSplitter, SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( FMargin( 0,0,0,1) )
				[
					SNew(SActorRuntimeDetailsUneditableComponentWarning)
					.Visibility(this, &SActorRuntimeDetails::GetUCSComponentWarningVisibility)
					.WarningText(NSLOCTEXT("SActorRuntimeDetails", "BlueprintUCSComponentWarning", "Components created by the User Construction Script can only be edited in the <a id=\"HyperlinkDecorator\" style=\"DetailsView.BPMessageHyperlinkStyle\">Blueprint</>"))
					.OnHyperlinkClicked(this, &SActorRuntimeDetails::OnBlueprintedComponentWarningHyperlinkClicked)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( FMargin( 0,0,0,1) )
				[
					SNew(SActorRuntimeDetailsUneditableComponentWarning)
					.Visibility(this, &SActorRuntimeDetails::GetInheritedBlueprintComponentWarningVisibility)
					.WarningText(NSLOCTEXT("SActorRuntimeDetails", "BlueprintUneditableInheritedComponentWarning", "Components flagged as not editable when inherited must be edited in the <a id=\"HyperlinkDecorator\" style=\"DetailsView.BPMessageHyperlinkStyle\">Blueprint</>"))
					.OnHyperlinkClicked(this, &SActorRuntimeDetails::OnBlueprintedComponentWarningHyperlinkClicked)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( FMargin( 0,0,0,1) )
				[
					SNew(SActorRuntimeDetailsUneditableComponentWarning)
					.Visibility(this, &SActorRuntimeDetails::GetNativeComponentWarningVisibility)
					.WarningText(NSLOCTEXT("SActorRuntimeDetails", "UneditableNativeComponentWarning", "Native components are editable when declared as a UProperty in <a id=\"HyperlinkDecorator\" style=\"DetailsView.BPMessageHyperlinkStyle\">C++</>"))
					.OnHyperlinkClicked(this, &SActorRuntimeDetails::OnNativeComponentWarningHyperlinkClicked)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					DetailsView->GetFilterAreaWidget().ToSharedRef()
				]
				+ SVerticalBox::Slot()
				[
					DetailsView.ToSharedRef()
				]
			]
		]
	];

	DetailsSplitter->AddSlot(0)
	.Value(.2f)
	[
		ComponentsBox.ToSharedRef()
	];
}

SActorRuntimeDetails::~SActorRuntimeDetails()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
	USelection::SelectionChangedEvent.RemoveAll(this);
	RemoveBPComponentCompileEventDelegate();

	FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
	if (LevelEditor != nullptr)
	{
		LevelEditor->OnComponentsEdited().RemoveAll(this);
	}
}

void SActorRuntimeDetails::OnDetailsViewObjectArrayChanged(const FString& InTitle, const TArray<UObject*>& InObjects)
{
	// The DetailsView will already check validity every tick and hide itself when invalid, so this piggy-backs on that code instead of needing a second tick function.
	if (InObjects.Num() == 0 && !LockedActorSelection.IsValid())
	{
		ComponentsBox->SetVisibility(EVisibility::Collapsed);
	}
}

void SActorRuntimeDetails::SetObjects(const TArray<UObject*>& InObjects, bool bForceRefresh)
{
	if (GEditor->PlayWorld == nullptr)
		return;

	if(!DetailsView->IsLocked())
	{
		DetailsView->SetObjects(InObjects, bForceRefresh);

		bool bShowingComponents = false;

		if(InObjects.Num() == 1 && FKismetEditorUtilities::CanCreateBlueprintOfClass(InObjects[0]->GetClass()))
		{
			AActor* Actor = GetSelectedActorInEditor();
			if(Actor)
			{
				LockedActorSelection = Actor;
				bShowingComponents = true;

				// Update the tree if a new actor is selected
				if(GEditor->GetSelectedComponentCount() == 0)
				{
					// Enable the selection guard to prevent OnTreeSelectionChanged() from altering the editor's component selection
					TGuardValue<bool> SelectionGuard(bSelectionGuard, true);
					SCSRuntimeEditor->UpdateTree();
				}
			}
		}

		ComponentsBox->SetVisibility(bShowingComponents ? EVisibility::Visible : EVisibility::Collapsed);

		if(DetailsView->GetHostTabManager().IsValid())
		{
			TSharedPtr<SDockTab> Tab = DetailsView->GetHostTabManager()->FindExistingLiveTab(DetailsView->GetIdentifier());
			if (Tab.IsValid() && !Tab->IsForeground() )
			{
				Tab->FlashTab();
			}
		}
	}
}

void SActorRuntimeDetails::PostUndo(bool bSuccess)
{
	if (GEditor->PlayWorld == nullptr)
		return;

	// Enable the selection guard to prevent OnTreeSelectionChanged() from altering the editor's component selection
	TGuardValue<bool> SelectionGuard(bSelectionGuard, true);

	if (!DetailsView->IsLocked())
	{
		// Make sure the locked actor selection matches the editor selection
		AActor* SelectedActor = GetSelectedActorInEditor();
		if (SelectedActor && SelectedActor != LockedActorSelection.Get())
		{
			LockedActorSelection = SelectedActor;
		}
	}
	

	// Refresh the tree and update the selection to match the world
	SCSRuntimeEditor->UpdateTree();
	UpdateComponentTreeFromEditorSelection();

	AActor* SelectedActor = GetSelectedActorInEditor();
	if (SelectedActor)
	{
		GUnrealEd->SetActorSelectionFlags(SelectedActor);

		// Update the pivot (widget) as the current selection may be a component within the Actor instance
		GUnrealEd->UpdatePivotLocationForSelection();
	}
}

void SActorRuntimeDetails::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

void SActorRuntimeDetails::NotifyPreChange(UProperty* PropertyAboutToChange)
{
	//Use bActorSeamlessTraveled to stop Actor Reconstruction
	AActor* Actor = GetActorContext();
	if (Actor)
		Actor->bActorSeamlessTraveled = true;
}

void SActorRuntimeDetails::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	AActor* Actor = GetActorContext();
	if (Actor)
		Actor->bActorSeamlessTraveled = false;
}

void SActorRuntimeDetails::OnComponentsEditedInWorld()
{
	if (GetSelectedActorInEditor() == GetActorContext())
	{
		// The component composition of the observed actor has changed, so rebuild the node tree
		TGuardValue<bool> SelectionGuard(bSelectionGuard, true);

		// Refresh the tree and update the selection to match the world
		SCSRuntimeEditor->UpdateTree();
	}
}

void SActorRuntimeDetails::OnEditorSelectionChanged(UObject* Object)
{
	if (GEditor->PlayWorld == nullptr)
		return;
	
	if(!bSelectionGuard && SCSRuntimeEditor.IsValid())
	{
		// Make sure the selection set that changed is relevant to us
		USelection* Selection = Cast<USelection>(Object);
		if(Selection == GEditor->GetSelectedComponents() || Selection == GEditor->GetSelectedActors())
		{
			UpdateComponentTreeFromEditorSelection();

			if(GEditor->GetSelectedComponentCount() == 0) // An actor was selected
			{
				// Ensure the selection flags are up to date for the components in the selected actor
				for(FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
				{
					AActor* Actor = CastChecked<AActor>(*It);
					GUnrealEd->SetActorSelectionFlags(Actor);
				}
			}
		}
	}
}

AActor* SActorRuntimeDetails::GetSelectedActorInEditor() const
{
	//@todo this doesn't work w/ multi-select
	return GEditor->GetSelectedActors()->GetTop<AActor>();
}

bool SActorRuntimeDetails::GetAllowComponentTreeEditing() const
{
	return true;
	//return GEditor->PlayWorld == nullptr;
}

AActor* SActorRuntimeDetails::GetActorContext() const
{
	if (GEditor->PlayWorld == nullptr)
		return nullptr;
	
	AActor* SelectedActorInEditor = GetSelectedActorInEditor();
	const bool bDetailsLocked = DetailsView->IsLocked();

	// If the details is locked or we have a valid locked selection that doesn't match the editor's selected actor, use the locked selection
	if (bDetailsLocked || ( LockedActorSelection.IsValid() && LockedActorSelection.Get() != SelectedActorInEditor ))
	{
		return LockedActorSelection.Get();
	}
	else
	{
		return SelectedActorInEditor;
	}
}

void SActorRuntimeDetails::OnSCSRuntimeEditorRootSelected(AActor* Actor)
{
	if(!bSelectionGuard)
	{
		GEditor->SelectNone(true, true, false);
		GEditor->SelectActor(Actor, true, true, true);
	}
}

void SActorRuntimeDetails::OnSCSRuntimeEditorTreeViewSelectionChanged(const TArray<FSCSRuntimeEditorTreeNodePtrType>& SelectedNodes)
{
	if (!bSelectionGuard && SelectedNodes.Num() > 0)
	{
		if( SelectedNodes.Num() > 1 && SelectedBPComponentBlueprint.IsValid() )
		{
			// Remove the compilation delegate if we are no longer displaying the full details for a single blueprint component.
			RemoveBPComponentCompileEventDelegate();
		}

		AActor* Actor = GetActorContext();
		if (Actor)
		{
			TArray<UObject*> DetailsObjects;

			// Determine if the root actor node is among the selected nodes and Count number of components selected
			bool bActorNodeSelected = false;
			int NumSelectedComponentNodes = 0;
			for (const FSCSRuntimeEditorTreeNodePtrType& SelectedNode : SelectedNodes)
			{
				if (SelectedNode.IsValid())
				{
					if (SelectedNode->GetNodeType() == FSCSRuntimeEditorTreeNode::RootActorNode)
					{
						bActorNodeSelected = true;
					}
					else if (SelectedNode->GetNodeType() == FSCSRuntimeEditorTreeNode::ComponentNode)
					{
						++NumSelectedComponentNodes;
					}
				}
			}

			if (DetailsView->IsLocked())
			{
				// When the details panel is locked, we don't want to touch the editor's component selection
				// We do want to force the locked panel to update to match the selected components, though, since they are part of the actor selection we're locked on

				if (bActorNodeSelected)
				{
					// If the actor root is selected, then the editor component selection should remain empty and we only show the Actor's details
					DetailsObjects.Add(Actor);
				}
				else
				{
					const bool bSingleComponentSelection = SelectedNodes.Num() == 1;

					for (const FSCSRuntimeEditorTreeNodePtrType& SelectedNode : SelectedNodes)
					{
						UActorComponent* ComponentInstance = SelectedNode->FindComponentInstanceInActor(Actor);
						if (ComponentInstance)
						{
							DetailsObjects.Add(ComponentInstance);

							if(bSingleComponentSelection)
							{
								// Add delegate to monitor blueprint component compilation if we have a full details view ( i.e. single selection )
								if(UBlueprintGeneratedClass* ComponentBPGC = Cast<UBlueprintGeneratedClass>(ComponentInstance->GetClass()))
								{
									if(UBlueprint* ComponentBlueprint = Cast<UBlueprint>(ComponentBPGC->ClassGeneratedBy))
									{
										AddBPComponentCompileEventDelegate(ComponentBlueprint);
									}
								}
							}
						}
					}
				}

				const bool bOverrideDetailsLock = true;
				DetailsView->SetObjects(DetailsObjects, false, bOverrideDetailsLock);
			}
			else
			{
				// Enable the selection guard to prevent OnEditorSelectionChanged() from altering the contents of the SCSTreeWidget
				TGuardValue<bool> SelectionGuard(bSelectionGuard, true);

				// Make sure the actor is selected in the editor (possible if the panel was just unlocked, but still assigned to the locked actor)
				if (!GEditor->GetSelectedActors()->IsSelected(Actor))
				{
					GEditor->SelectNone(false, true, false);
					GEditor->SelectActor(Actor, true, true, true);
				}

				USelection* SelectedComponents = GEditor->GetSelectedComponents();

				// Determine if the selected non-root actor nodes differ from the editor component selection
				bool bComponentSelectionChanged = GEditor->GetSelectedComponentCount() != NumSelectedComponentNodes;
				if (!bComponentSelectionChanged)
				{
					// Check to see if any of the selected nodes aren't already selected in the world
					for (const FSCSRuntimeEditorTreeNodePtrType& SelectedNode : SelectedNodes)
					{
						if (SelectedNode.IsValid() && SelectedNode->GetNodeType() == FSCSRuntimeEditorTreeNode::ComponentNode)
						{
							UActorComponent* ComponentInstance = SelectedNode->FindComponentInstanceInActor(Actor);
							if (ComponentInstance && !SelectedComponents->IsSelected(ComponentInstance))
							{
								bComponentSelectionChanged = true;
								break;
							}
						}
					}
				}

				// Does the actor selection differ from our previous state?
				const bool bActorSelectionChanged = bShowingRootActorNodeSelected != bActorNodeSelected;

				// If necessary, update the editor component selection
				if (bActorSelectionChanged || ( bComponentSelectionChanged && !bActorNodeSelected ))
				{
					// Store whether we're now showing the actor root as selected
					bShowingRootActorNodeSelected = bActorNodeSelected;

					// Note: this transaction should not take place if we are in the middle of executing an undo or redo because it would clear the top of the transaction stack.
					const bool bShouldActuallyTransact = !GIsTransacting;
					const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ClickingOnComponentInTree", "Clicking on Component (tree view)"), bShouldActuallyTransact);

					// Dirty the actor selection so it stays in sync with the component selection
					GEditor->GetSelectedActors()->Modify();
					// Update the editor's component selection to match the node selection
					SelectedComponents->Modify();
					SelectedComponents->BeginBatchSelectOperation();
					SelectedComponents->DeselectAll();

					if (bShowingRootActorNodeSelected)
					{
						// If the actor root is selected, then the editor component selection should remain empty and we only show the Actor's details
						DetailsObjects.Add(Actor);
					}
					else
					{
						const bool bSingleComponentSelection = SelectedNodes.Num() == 1;

						for (const FSCSRuntimeEditorTreeNodePtrType& SelectedNode : SelectedNodes)
						{
							if (SelectedNode.IsValid())
							{
								UActorComponent* ComponentInstance = SelectedNode->FindComponentInstanceInActor(Actor);
								if (ComponentInstance)
								{
									DetailsObjects.Add(ComponentInstance);
									SelectedComponents->Select(ComponentInstance);

									if(bSingleComponentSelection)
									{
										// Add delegate to monitor blueprint component compilation if we have a full details view ( i.e. single selection )
										if(UBlueprintGeneratedClass* ComponentBPGC = Cast<UBlueprintGeneratedClass>(ComponentInstance->GetClass()))
										{
											if(UBlueprint* ComponentBlueprint = Cast<UBlueprint>(ComponentBPGC->ClassGeneratedBy))
											{
												AddBPComponentCompileEventDelegate(ComponentBlueprint);
											}
										}
									}
									// Ensure the selection override is bound for this component (including any attached editor-only children)
									USceneComponent* SceneComponent = Cast<USceneComponent>(ComponentInstance);
									if (SceneComponent)
									{
										FComponentEditorUtils::BindComponentSelectionOverride(SceneComponent, true);
									}
								}
							}
						}
					}

					SelectedComponents->EndBatchSelectOperation();

					DetailsView->SetObjects(DetailsObjects);

					GUnrealEd->SetActorSelectionFlags(Actor);
					GUnrealEd->UpdatePivotLocationForSelection(true);
					GEditor->RedrawLevelEditingViewports();
				}
			}
		}
	}
}

void SActorRuntimeDetails::OnSCSRuntimeEditorTreeViewItemDoubleClicked(const TSharedPtr<class FSCSRuntimeEditorTreeNode> ClickedNode)
{
	if (ClickedNode.IsValid())
	{
		if (ClickedNode->GetNodeType() == FSCSRuntimeEditorTreeNode::ComponentNode)
		{
			USceneComponent* SceneComponent = Cast<USceneComponent>(ClickedNode->GetComponentTemplate());
			if (SceneComponent != nullptr)
			{
				const bool bActiveViewportOnly = false;
				GEditor->MoveViewportCamerasToComponent(SceneComponent, bActiveViewportOnly);
			}
		}
	}
}

void SActorRuntimeDetails::UpdateComponentTreeFromEditorSelection()
{
	if (GEditor->PlayWorld == nullptr)
		return;

	if (!DetailsView->IsLocked())
	{
		// Enable the selection guard to prevent OnTreeSelectionChanged() from altering the editor's component selection
		TGuardValue<bool> SelectionGuard(bSelectionGuard, true);

		TSharedPtr<SSCSTreeType>& SCSTreeWidget = SCSRuntimeEditor->SCSTreeWidget;
		TArray<UObject*> DetailsObjects;

		// Update the tree selection to match the level editor component selection
		SCSTreeWidget->ClearSelection();
		for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
		{
			UActorComponent* Component = CastChecked<UActorComponent>(*It);

			FSCSRuntimeEditorTreeNodePtrType SCSTreeNode = SCSRuntimeEditor->GetNodeFromActorComponent(Component, false);
			if (SCSTreeNode.IsValid() && SCSTreeNode->GetComponentTemplate())
			{
				SCSTreeWidget->RequestScrollIntoView(SCSTreeNode);
				SCSTreeWidget->SetItemSelection(SCSTreeNode, true);

				UActorComponent* ComponentTemplate = SCSTreeNode->GetComponentTemplate();
				check(Component == ComponentTemplate);
				DetailsObjects.Add(Component);
			}
		}

		if (DetailsObjects.Num() > 0)
		{
			DetailsView->SetObjects(DetailsObjects, bSelectedComponentRecompiled);
		}
		else
		{
			SCSRuntimeEditor->SelectRoot();
		}
	}
}

bool SActorRuntimeDetails::IsPropertyReadOnly(const FPropertyAndParent& PropertyAndParent) const
{
	return false;
	
	//bool bIsReadOnly = false;
	//for (const FSCSRuntimeEditorTreeNodePtrType& Node : SCSRuntimeEditor->GetSelectedNodes())
	//{
	//	UActorComponent* Component = Node->GetComponentTemplate();
	//	if (Component && Component->CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
	//	{
	//		TSet<const UProperty*> UCSModifiedProperties;
	//		Component->GetUCSModifiedProperties(UCSModifiedProperties);
	//		if (UCSModifiedProperties.Contains(&PropertyAndParent.Property) || (PropertyAndParent.ParentProperty && UCSModifiedProperties.Contains(PropertyAndParent.ParentProperty)))
	//		{
	//			bIsReadOnly = true;
	//			break;
	//		}
	//	}
	//}
	//return bIsReadOnly;
}

bool SActorRuntimeDetails::IsPropertyEditingEnabled() const
{
	return true;
	
	//FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	//if (!LevelEditor.AreObjectsEditable(DetailsView->GetSelectedObjects()))
	//{
	//	return false;
	//}

	//bool bIsEditable = true;
	//for (const FSCSRuntimeEditorTreeNodePtrType& Node : SCSRuntimeEditor->GetSelectedNodes())
	//{
	//	bIsEditable = Node->CanEditDefaults() || Node->GetNodeType() == FSCSRuntimeEditorTreeNode::ENodeType::RootActorNode;
	//	if (!bIsEditable)
	//	{
	//		break;
	//	}
	//}
	//return bIsEditable;
}

void SActorRuntimeDetails::OnBlueprintedComponentWarningHyperlinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
	UBlueprint* Blueprint = SCSRuntimeEditor->GetBlueprint();
	if (Blueprint)
	{
		// Open the blueprint
		GEditor->EditObject(Blueprint);
	}
}

void SActorRuntimeDetails::OnNativeComponentWarningHyperlinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
	// Find the closest native parent
	UBlueprint* Blueprint = SCSRuntimeEditor->GetBlueprint();
	UClass* ParentClass = Blueprint ? *Blueprint->ParentClass : GetActorContext()->GetClass();
	while (ParentClass && !ParentClass->HasAllClassFlags(CLASS_Native))
	{
		ParentClass = ParentClass->GetSuperClass();
	}

	if (ParentClass)
	{
		FString NativeParentClassHeaderPath;
		const bool bFileFound = FSourceCodeNavigation::FindClassHeaderPath(ParentClass, NativeParentClassHeaderPath)
			&& ( IFileManager::Get().FileSize(*NativeParentClassHeaderPath) != INDEX_NONE );
		if (bFileFound)
		{
			const FString AbsoluteHeaderPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*NativeParentClassHeaderPath);
			FSourceCodeNavigation::OpenSourceFile(AbsoluteHeaderPath);
		}
	}
}

EVisibility SActorRuntimeDetails::GetPlayInGameTipVisibility() const
{
	return GEditor->PlayWorld == nullptr ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SActorRuntimeDetails::GetUCSComponentWarningVisibility() const
{
	return EVisibility::Collapsed;
	
	//bool bIsUneditableBlueprintComponent = false;

	//// Check to see if any selected components are inherited from blueprint
	//for (const FSCSRuntimeEditorTreeNodePtrType& Node : SCSRuntimeEditor->GetSelectedNodes())
	//{
	//	if (!Node->IsNative())
	//	{
	//		UActorComponent* Component = Node->GetComponentTemplate();
	//		bIsUneditableBlueprintComponent = Component ? Component->CreationMethod == EComponentCreationMethod::UserConstructionScript : false;
	//		if (bIsUneditableBlueprintComponent)
	//		{
	//			break;
	//		}
	//	}
	//}

	//return bIsUneditableBlueprintComponent ? EVisibility::Visible : EVisibility::Collapsed;
}

bool NotEditableSetByBlueprint(UActorComponent* Component)
{
	// Determine if it is locked out from a blueprint or from the native
	UActorComponent* Archetype = CastChecked<UActorComponent>(Component->GetArchetype());
	while (Archetype)
	{
		if (Archetype->GetOuter()->IsA<UBlueprintGeneratedClass>() || Archetype->GetOuter()->GetClass()->HasAllClassFlags(CLASS_CompiledFromBlueprint))
		{
			if (!Archetype->bEditableWhenInherited)
			{
				return true;
			}

			Archetype = CastChecked<UActorComponent>(Archetype->GetArchetype());
		}
		else
		{
			Archetype = nullptr;
		}
	}

	return false;
}

EVisibility SActorRuntimeDetails::GetInheritedBlueprintComponentWarningVisibility() const
{
	return EVisibility::Collapsed;

	//bool bIsUneditableBlueprintComponent = false;

	//// Check to see if any selected components are inherited from blueprint
	//for (const FSCSRuntimeEditorTreeNodePtrType& Node : SCSRuntimeEditor->GetSelectedNodes())
	//{
	//	if (!Node->IsNative())
	//	{
	//		if (UActorComponent* Component = Node->GetComponentTemplate())
	//		{
	//			if (!Component->IsEditableWhenInherited() && Component->CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
	//			{
	//				bIsUneditableBlueprintComponent = true;
	//				break;
	//			}
	//		}
	//	}
	//	else if (!Node->CanEditDefaults() && NotEditableSetByBlueprint(Node->GetComponentTemplate()))
	//	{
	//		bIsUneditableBlueprintComponent = true;
	//		break;
	//	}
	//}

	//return bIsUneditableBlueprintComponent ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SActorRuntimeDetails::GetNativeComponentWarningVisibility() const
{
	return EVisibility::Collapsed;
	
	//bool bIsUneditableNative = false;
	//for (const FSCSRuntimeEditorTreeNodePtrType& Node : SCSRuntimeEditor->GetSelectedNodes())
	//{
	//	// Check to see if the component is native and not editable
	//	if (Node->IsNative() && !Node->CanEditDefaults() && !NotEditableSetByBlueprint(Node->GetComponentTemplate()))
	//	{
	//		bIsUneditableNative = true;
	//		break;
	//	}
	//}
	//
	//return bIsUneditableNative ? EVisibility::Visible : EVisibility::Collapsed;
}

void SActorRuntimeDetails::AddBPComponentCompileEventDelegate(UBlueprint* ComponentBlueprint)
{
	if(SelectedBPComponentBlueprint.Get() != ComponentBlueprint)
	{
		RemoveBPComponentCompileEventDelegate();
		SelectedBPComponentBlueprint = ComponentBlueprint;
		// Add blueprint component compilation event delegate
		if(!ComponentBlueprint->OnCompiled().IsBoundToObject(this))
		{
			ComponentBlueprint->OnCompiled().AddSP(this, &SActorRuntimeDetails::OnBlueprintComponentCompiled);
		}
	}
}

void SActorRuntimeDetails::RemoveBPComponentCompileEventDelegate()
{
	// Remove blueprint component compilation event delegate
	if(SelectedBPComponentBlueprint.IsValid())
	{
		SelectedBPComponentBlueprint.Get()->OnCompiled().RemoveAll(this);
		SelectedBPComponentBlueprint.Reset();
		bSelectedComponentRecompiled = false;
	}
}

void SActorRuntimeDetails::OnBlueprintComponentCompiled(UBlueprint* ComponentBlueprint)
{
	if (GEditor->PlayWorld == nullptr)
		return;

	bSelectedComponentRecompiled = true;
	UpdateComponentTreeFromEditorSelection();
	bSelectedComponentRecompiled = false;
}
