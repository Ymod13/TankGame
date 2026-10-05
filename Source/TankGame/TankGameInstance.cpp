// Created by Domenico Guaccero - 2026


#include "TankGameInstance.h"
#include "TankProjectile.h"
#include <Kismet/GameplayStatics.h>
#include "PDUConversions_BPFL.h"

ADISGameManager *UTankGameInstance::GetDisManager()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	LevelDISManager = ADISGameManager::GetDISGameManager(World);

	if (!LevelDISManager)
	{
		UE_LOG(LogTemp, Error, TEXT("UTankGameInstance: ADISGameManager not FOUND in the level."));
		return nullptr;
	}

	return LevelDISManager;
}
//---------------------------------------------------------------------------------------------------------------------

void UTankGameInstance::AddGhostProjectile(ATankProjectile* NewGhostProjectile)
{
	ATankProjectile *&FoundProjectile = GhostProjectiles.FindOrAdd(NewGhostProjectile->EventNumber);
	if (FoundProjectile == nullptr)
	{
		FoundProjectile = NewGhostProjectile;
		UE_LOG(LogTemp, Log, TEXT("AddGhostProjectile: Added ghost projectile for evend id:%d"), NewGhostProjectile->EventNumber);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AddGhostProjectile: Projectile already present for evend id %d (Check)"), NewGhostProjectile->EventNumber);
	}
}
//---------------------------------------------------------------------------------------------------------------------

void UTankGameInstance::RemoveGhostProjectile(const int32 ProjectileEventNumber)
{
	ATankProjectile** FoundProjectileRef = GhostProjectiles.Find(ProjectileEventNumber);
	
	if (FoundProjectileRef != nullptr)
	{
		if (ATankProjectile* FoundProjectile = *FoundProjectileRef)
		{
			FoundProjectile->Destroy();
			GhostProjectiles.Remove(ProjectileEventNumber);
			UE_LOG(LogTemp, Log, TEXT("RemoveGhostProjectile: Ghost projectile succesfully removed for evend id: %d"), ProjectileEventNumber);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("RemoveGhostProjectile: Ghost projectile for event number %d found! but Projectile not valid -> Impossible to remove it! (map size: %d)"), ProjectileEventNumber, GhostProjectiles.Num());
		}
		
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveGhostProjectile: Ghost projectile for event number %d not found! Impossible to remove it! (map size: %d)"), ProjectileEventNumber, GhostProjectiles.Num());
	}
}
//---------------------------------------------------------------------------------------------------------------------