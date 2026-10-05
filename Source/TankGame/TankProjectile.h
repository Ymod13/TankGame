// Created by Domenico Guaccero - 2026

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class ATankPawn;

UCLASS()
class TANKGAME_API ATankProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATankProjectile();

	// bInAuthoritative = true: real local projectile (with collisions).
	// false: cosmetic projectile (no collision).
	void InitProjectile(ATankPawn* InShooter, bool bInAuthoritative);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float Speed = 4000.f; 

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float LifeSeconds = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float Damage = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<class AActor> ExplosionVfxClass;

	int32 EventNumber = 0;

protected:
	UPROPERTY(VisibleAnywhere) USphereComponent* Collision;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere) UProjectileMovementComponent* Movement;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	TWeakObjectPtr<ATankPawn> Shooter;
};