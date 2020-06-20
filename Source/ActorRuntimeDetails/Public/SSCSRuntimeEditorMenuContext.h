// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SSCSRuntimeEditorMenuContext.generated.h"

class SSCSRuntimeEditor;

UCLASS()
class USSCSRuntimeEditorMenuContext : public UObject
{
	GENERATED_BODY()
public:
	
	TWeakPtr<SSCSRuntimeEditor> SCSRuntimeEditor;

	bool bOnlyShowPasteOption;
};
