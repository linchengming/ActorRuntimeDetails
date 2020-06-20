// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARDUEFeatures.h"
#include "GameFramework/Actor.h"

class FRuntimeDetailsEditorUtils
{
public:
	/**
	 * Deletes the indicated components and identifies the component that should be selected following the operation.
	 * Note: Does not take care of the actual selection of a new component. It only identifies which component should be selected.
	 * 
	 * @param ComponentsToDelete The list of components to delete
	 * @param OutComponentToSelect The component that should be selected after the deletion
	 * @return The number of components that were actually deleted
	 */
	static int32 DeleteComponents(const TArray<UActorComponent*>& ComponentsToDelete, UActorComponent*& OutComponentToSelect);

	/** Helper method to rename the given component template along with any instances */
	static void RenameComponentTemplate(UActorComponent* ComponentTemplate, const FName& NewName);

	/**
	 * Test whether or not the given string is already the name string of a component on the the owner
	 * Optionally excludes an existing component from the check (ex. a component currently being renamed)
	 * @return True if the InString is an available name for a component of ComponentOwner
	 */
	static bool IsComponentNameAvailable(const FString & InString, UObject * ComponentOwner, const UActorComponent * ComponentToIgnore);

	static FVector& GetRelativeLocation_DirectMutable(USceneComponent* SceneComponent);
	static FVector GetRelativeLocation(USceneComponent* SceneComponent);
	static void SetRelativeLocation(USceneComponent* SceneComponent, FVector RelativeLocation);

	static FRotator& GetRelativeRotation_DirectMutable(USceneComponent* SceneComponent);
	static FRotator GetRelativeRotation(USceneComponent* SceneComponent);
	static void SetRelativeRotation(USceneComponent* SceneComponent, FRotator RelativeRotation);

	static FVector& GetRelativeScale3D_DirectMutable(USceneComponent* SceneComponent);
	static FVector GetRelativeScale3D(USceneComponent* SceneComponent);
	static void SetRelativeScale3D(USceneComponent* SceneComponent, FVector RelativeScale3D);

	static bool IsUsingAbsoluteLocation(USceneComponent* SceneComponent);
	static bool IsUsingAbsoluteRotation(USceneComponent* SceneComponent);
	static bool IsUsingAbsoluteScale(USceneComponent* SceneComponent);
};
