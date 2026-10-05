// Created by Domenico Guaccero - 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TankGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class TANKGAME_API ATankGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATankGameModeBase();

	template<typename T>
	static T* PickRandom(const TArray<T*>& Array)
	{
		return Array.Num() > 0 ? Array[FMath::RandRange(0, Array.Num() - 1)] : nullptr;
	}

	UFUNCTION(BlueprintCallable, Category = "Tank|Score")
	AActor* PickRandomStartPoint();

protected:

	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	UPROPERTY(EditDefaultsOnly, Category = "DIS Setup")
	TSubclassOf<class ATankPawn> TankClass;

	TArray<AActor*> FoundPlayerStarts;

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
