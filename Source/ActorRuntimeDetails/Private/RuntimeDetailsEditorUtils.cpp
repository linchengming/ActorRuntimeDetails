// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RuntimeDetailsEditorUtils.h"

#define LOCTEXT_NAMESPACE "FRuntimeDetailsEditorUtilsEditorUtils"

int32 FRuntimeDetailsEditorUtils::DeleteComponents(const TArray<UActorComponent*>& ComponentsToDelete, UActorComponent*& OutComponentToSelect)
{
	int32 NumDeletedComponents = 0;

	for (UActorComponent* ComponentToDelete : ComponentsToDelete)
	{
		AActor* Owner = ComponentToDelete->GetOwner();
		check(Owner != nullptr);

		// If necessary, determine the component that should be selected following the deletion of the indicated component
		if (!OutComponentToSelect || ComponentToDelete == OutComponentToSelect)
		{
			USceneComponent* RootComponent = Owner->GetRootComponent();
			if (RootComponent != ComponentToDelete)
			{
				// Worst-case, the root can be selected
				OutComponentToSelect = RootComponent;

				if (USceneComponent* ComponentToDeleteAsSceneComp = Cast<USceneComponent>(ComponentToDelete))
				{
					if (USceneComponent* ParentComponent = ComponentToDeleteAsSceneComp->GetAttachParent())
					{
						// The component to delete has a parent, so we select that in the absence of an appropriate sibling
						OutComponentToSelect = ParentComponent;

						// Try to select the sibling that immediately precedes the component to delete
						TArray<USceneComponent*> Siblings;
						ParentComponent->GetChildrenComponents(false, Siblings);
						for (int32 i = 0; i < Siblings.Num() && ComponentToDelete != Siblings[i]; ++i)
						{
							if (Siblings[i] && !Siblings[i]->IsPendingKill())
							{
								OutComponentToSelect = Siblings[i];
							}
						}
					}
				}
				else
				{
					// For a non-scene component, try to select the preceding non-scene component
					for (UActorComponent* Component : Owner->GetComponents())
					{
						if (Component != nullptr)
						{
							if (Component == ComponentToDelete)
							{
								break;
							}
							else if (!Component->IsA<USceneComponent>())
							{
								OutComponentToSelect = Component;
							}
						}
					}
				}
			}
			else
			{
				OutComponentToSelect = nullptr;
			}
		}

		// Actually delete the component
		ComponentToDelete->Modify();
		ComponentToDelete->DestroyComponent(true);
		NumDeletedComponents++;
	}

	// Non-native components will be reinstanced, so we have to update the ptr after reconstruction
	// in order to avoid pointing at an invalid (trash) instance after re-running construction scripts.
	FName ComponentToSelectName;
	const AActor* ComponentToSelectOwner = nullptr;
	if (OutComponentToSelect && OutComponentToSelect->CreationMethod != EComponentCreationMethod::Native)
	{
		// Keep track of the pending selection's name and owner
		ComponentToSelectName = OutComponentToSelect->GetFName();
		ComponentToSelectOwner = OutComponentToSelect->GetOwner();

		// Reset the ptr value - we'll reassign it after reconstruction
		OutComponentToSelect = nullptr;
	}

	return NumDeletedComponents;
}

void FRuntimeDetailsEditorUtils::RenameComponentTemplate(UActorComponent* ComponentTemplate, const FName& NewName)
{
	if (ComponentTemplate != nullptr)
	{
		// Rename the component template (archetype) - note that this can be called during compile-on-load, so we include the flag not to reset the BPGC's package loader.
		const FString NewComponentName = NewName.ToString();
		ComponentTemplate->Rename(*(NewComponentName), nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	}
}

bool FRuntimeDetailsEditorUtils::IsComponentNameAvailable(const FString& InString, UObject* ComponentOwner, const UActorComponent* ComponentToIgnore)
{
	UObject* Object = FindObjectFast<UObject>(ComponentOwner, *InString);

	bool bNameIsAvailable = Object == nullptr || Object == ComponentToIgnore;

	return bNameIsAvailable;
}

FVector& FRuntimeDetailsEditorUtils::GetRelativeLocation_DirectMutable(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->GetRelativeLocation_DirectMutable();
#else
	return SceneComponent->RelativeLocation;
#endif
}

FVector FRuntimeDetailsEditorUtils::GetRelativeLocation(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->GetRelativeLocation();
#else
	return SceneComponent->RelativeLocation;
#endif
}

void FRuntimeDetailsEditorUtils::SetRelativeLocation(USceneComponent* SceneComponent, FVector RelativeLocation)
{
#if UE_4_24_OR_LATER
	SceneComponent->SetRelativeLocation(RelativeLocation);
#else
	SceneComponent->RelativeLocation = RelativeLocation;
#endif
}

FRotator&  FRuntimeDetailsEditorUtils::GetRelativeRotation_DirectMutable(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->GetRelativeRotation_DirectMutable();
#else
	return SceneComponent->RelativeRotation;
#endif
}

FRotator FRuntimeDetailsEditorUtils::GetRelativeRotation(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->GetRelativeRotation();
#else
	return SceneComponent->RelativeRotation;
#endif
}

void FRuntimeDetailsEditorUtils::SetRelativeRotation(USceneComponent* SceneComponent, FRotator RelativeRotation)
{
#if UE_4_24_OR_LATER
	SceneComponent->SetRelativeRotation(RelativeRotation);
#else
	SceneComponent->RelativeRotation = RelativeRotation;
#endif
}

FVector& FRuntimeDetailsEditorUtils::GetRelativeScale3D_DirectMutable(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->GetRelativeScale3D_DirectMutable();
#else
	return SceneComponent->RelativeScale3D;
#endif
}

FVector FRuntimeDetailsEditorUtils::GetRelativeScale3D(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->GetRelativeScale3D();
#else
	return SceneComponent->RelativeScale3D;
#endif
}

void FRuntimeDetailsEditorUtils::SetRelativeScale3D(USceneComponent* SceneComponent, FVector RelativeScale3D)
{
#if UE_4_24_OR_LATER
	SceneComponent->SetRelativeScale3D(RelativeScale3D);
#else
	SceneComponent->RelativeScale3D = RelativeScale3D;
#endif
}

bool FRuntimeDetailsEditorUtils::IsUsingAbsoluteLocation(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->IsUsingAbsoluteLocation();
#else
	return SceneComponent->bAbsoluteLocation;
#endif
}

bool FRuntimeDetailsEditorUtils::IsUsingAbsoluteRotation(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->IsUsingAbsoluteRotation();
#else
	return SceneComponent->bAbsoluteRotation;
#endif
}

bool FRuntimeDetailsEditorUtils::IsUsingAbsoluteScale(USceneComponent* SceneComponent)
{
#if UE_4_24_OR_LATER
	return SceneComponent->IsUsingAbsoluteScale();
#else
	return SceneComponent->bAbsoluteScale;
#endif
}

#undef LOCTEXT_NAMESPACE
