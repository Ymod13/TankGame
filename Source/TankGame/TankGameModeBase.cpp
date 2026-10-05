// Created by Domenico Guaccero - 2026


#include "TankGameModeBase.h"

#include "TankPawn.h"
#include "TankPlayerController.h"
#include "TankPlayerState.h"
#include "TankGameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include <Kismet/GameplayStatics.h>
#include "DISSendComponent.h"
#include "DISGameManager.h"
#include "TankGameInstance.h"
#include "Engine/TargetPoint.h"

ATankGameModeBase::ATankGameModeBase()
{
	DefaultPawnClass = ATankPawn::StaticClass();
	PlayerControllerClass = ATankPlayerController::StaticClass();
	PlayerStateClass = ATankPlayerState::StaticClass();
	GameStateClass = ATankGameStateBase::StaticClass();
}
//---------------------------------------------------------------------------------------------------------------------

AActor *ATankGameModeBase::PickRandomStartPoint()
{
	return PickRandom<AActor>(FoundPlayerStarts);
}
//---------------------------------------------------------------------------------------------------------------------

APawn* ATankGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// 1. reads "?PlayerID=" from stand alone command launch options
	const FString Opt = UGameplayStatics::ParseOption(OptionsString, TEXT("PlayerID"));
	const int32 Id = Opt.IsEmpty() ? 1 : FCString::Atoi(*Opt);

	if (ADISGameManager* Mgr = ADISGameManager::GetDISGameManager(this))
	{
		Mgr->ApplicationID = Id; 
	}

	FActorSpawnParameters Unused;
	ATankPawn* Tank = GetWorld()->SpawnActorDeferred<ATankPawn>(
		DefaultPawnClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (Tank)
	{
		Tank->SetPlayerId(Id);

		if (Tank->DISSendComponent)
		{
			Tank->DISSendComponent->EntityID = Id;
		}

		Tank->SetTankMesh(Id);

		Tank->FinishSpawning(SpawnTransform);
	}
	return Tank;
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	UTankGameInstance* GI = Cast<UTankGameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("ATankGameModeBase: Tank Game Instance not found!"));
		return;
	}

	GI->GetDisManager();
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ATankGameModeBase: World not found!"));
		return;
	}

	if (!NewPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("ATankGameModeBase: Local Player Controller not found!"));
		return;
	}

	const FString Opt = UGameplayStatics::ParseOption(OptionsString, TEXT("PlayerID"));
	const int32 Id = Opt.IsEmpty() ? 1 : FCString::Atoi(*Opt);

	ATankPawn* SpawnedTank = Cast<ATankPawn>(NewPlayer->GetPawn());

	if (!SpawnedTank)
	{
		UE_LOG(LogTemp, Error, TEXT("ATankGameModeBase: Tank Pawn not found not found!"));
		return;
	}

}
//---------------------------------------------------------------------------------------------------------------------

AActor* ATankGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	FString PlayerIDOption = UGameplayStatics::ParseOption(OptionsString, TEXT("PlayerID"));

	FoundPlayerStarts.Reset();

	FString StartPositionTag = "Player" + PlayerIDOption;
	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ATargetPoint::StaticClass(), FName(StartPositionTag), FoundPlayerStarts);

	AActor* StartTargetPoint = PickRandom<AActor>(FoundPlayerStarts);

	if (StartTargetPoint)
	{
		return StartTargetPoint;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ATankGameModeBase: Impossible to find a valid index for player start spawning points."));
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}
//---------------------------------------------------------------------------------------------------------------------