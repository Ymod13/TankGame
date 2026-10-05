// Created by Domenico Guaccero - 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DISGameManager.h"
#include "TankGameInstance.generated.h"

class ATankProjectile;

UCLASS()
class TANKGAME_API UTankGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	// gets the dis manager and stores the pointer into game instance
	ADISGameManager *GetDisManager();

	void AddGhostProjectile(ATankProjectile* NewGhostProjectile);
	void RemoveGhostProjectile(const int32 ProjectileEventNumber);

private:
	UPROPERTY()
	ADISGameManager* LevelDISManager;

	TMap<int32, ATankProjectile*> GhostProjectiles;
};
