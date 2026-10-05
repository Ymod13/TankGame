// Created by Domenico Guaccero - 2026

#pragma once

#include "CoreMinimal.h"
#include "DISGameManager.h"
#include "TankGameDISGameManager.generated.h"

/**
 * 
 */
UCLASS()
class TANKGAME_API ATankGameDISGameManager : public ADISGameManager
{
	GENERATED_BODY()

protected:

	virtual void BeginPlay() override;

};
