// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ActorRuntimeDetailsCommands.h"

#define LOCTEXT_NAMESPACE "FActorRuntimeDetailsModule"

void FActorRuntimeDetailsCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "ActorRuntimeDetails", "Execute ActorRuntimeDetails action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
