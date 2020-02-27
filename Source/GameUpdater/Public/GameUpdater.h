// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameUpdaterLog.h"

// engine header
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGameUpdaterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
