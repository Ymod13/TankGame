// Created by Domenico Guaccero - 2026


#include "TankProjectile.h"
#include "TankPawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

// Sets default values
ATankProjectile::ATankProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(10.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true); // per OnComponentHit
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->ProjectileGravityScale = 0.f;
	Movement->InitialSpeed = 0.f; 
	Movement->MaxSpeed = 0.f;

}
//---------------------------------------------------------------------------------------------------------------------

void ATankProjectile::InitProjectile(ATankPawn* InShooter, bool bInAuthoritative)
{
	Shooter = InShooter;
	SetLifeSpan(LifeSeconds);
	Movement->Velocity = GetActorForwardVector() * Speed;

	if (bInAuthoritative)
	{
		if (InShooter)
		{
			Collision->IgnoreActorWhenMoving(InShooter, true);

			if (UPrimitiveComponent* ShooterPrim = Cast<UPrimitiveComponent>(InShooter->GetRootComponent()))
			{
				ShooterPrim->IgnoreActorWhenMoving(this, true);
			}
		}
		Collision->OnComponentHit.AddDynamic(this, &ATankProjectile::OnProjectileHit);
	}
	else
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (IsValid(ExplosionVfxClass))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		// Cosmetic explosion
		GetWorld()->SpawnActor<AActor>(ExplosionVfxClass, GetActorTransform(), Params);
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!Other || Other == Shooter || Other == this)
	{
		return;
	}

	if (Shooter.IsValid())
	{
		Shooter->HandleProjectileHit(Other, Hit.ImpactPoint, Speed, Damage, EventNumber);
	}
	
	Destroy();
}
//---------------------------------------------------------------------------------------------------------------------

