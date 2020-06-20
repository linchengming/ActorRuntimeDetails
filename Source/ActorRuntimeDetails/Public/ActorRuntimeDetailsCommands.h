// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ActorRuntimeDetailsStyle.h"

class FActorRuntimeDetailsCommands : public TCommands<FActorRuntimeDetailsCommands>
{
public:

	FActorRuntimeDetailsCommands()
		: TCommands<FActorRuntimeDetailsCommands>(TEXT("ActorRuntimeDetails"), NSLOCTEXT("Contexts", "ActorRuntimeDetails", "ActorRuntimeDetails Plugin"), NAME_None, FActorRuntimeDetailsStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
