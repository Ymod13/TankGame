// Created by Domenico Guaccero - 2026


#include "TankPawn.h"

#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include <Kismet/GameplayStatics.h>
#include "TankProjectile.h"
#include "TankGameInstance.h"

#include "DISSendComponent.h"
#include "DISReceiveComponent.h"
#include "DISGameManager.h"
#include "DIS_BPFL.h"
#include "PDUConversions_BPFL.h"
#include "UDPSubsystem.h"
#include "GeoReferencingSystem.h"
#include "TankGameStateBase.h"


// Sets default values
ATankPawn::ATankPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	TankMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TankMesh"));
	SetRootComponent(TankMesh);
	TankMesh->SetMobility(EComponentMobility::Movable);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(TankMesh);
	Muzzle->SetRelativeLocation(FVector(200.f, 0.f, 80.f));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(TankMesh);
	SpringArm->TargetArmLength = 800.f;
	SpringArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	// 2 dis componets are always created; 
	// we decide in BeginPlay which one to activate based on bIsLocallyControlled, 
	// so we don't have to manage them with scattered logic elsewhere.
	
	DISSendComponent = CreateDefaultSubobject<UDISSendComponent>(TEXT("DISSendComponent"));
	DISReceiveComponent = CreateDefaultSubobject<UDISReceiveComponent>(TEXT("DISReceiveComponent"));
}
//---------------------------------------------------------------------------------------------------------------------
void ATankPawn::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	ADISGameManager* DisManager = ADISGameManager::GetDISGameManager(this);

	if (!DisManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("TankPawn: BeginPlay(): No Dis manager found in the level."));
		return;
	}

	if (bIsLocallyControlled)
	{
		// Tank movement will be sent to the other clients via DISSendComponent, and the tank will not receive any movement updates from the network.
		if (DISSendComponent)
		{
			DISSendComponent->SetActive(true);
		}
		if (DISReceiveComponent)
		{
			DISReceiveComponent->SetActive(false);
		}

		// Adds the Mapping Context to the Enhanced Input subsystem of the local player.
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
					LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					if (TankMappingContext)
					{
						Subsystem->AddMappingContext(TankMappingContext, /*Priority=*/0);
					}
				}
			}
		}

		// Starting view (TopDown). next tick, so PlayerController has already finised to set its view target on pawn
		GetWorldTimerManager().SetTimerForNextTick(this, &ATankPawn::ApplyCameraMode);

		DisManager->OnNoSpecificEntityDetonationPDUReceived.AddDynamic(this, &ATankPawn::OnDetonationReceived);
	}
	else
	{
		// Enemy "Ghost" tank: no real input
		// movement comes from DeadReckoning on DISReceiveComponent.
		
		if (DISSendComponent)
		{
			DISSendComponent->SetActive(false);
		}
		if (DISReceiveComponent) 
		{
			DISReceiveComponent->SetActive(true);

			DISReceiveComponent->ApplyToOwner = true;

			// this is an echo of the local tank created due to loopback, I hide it so the manager goes on mapping it and getting its next PDU
			const FEntityID& Id = DISReceiveComponent->EntityID;
			if (Id.Site == DisManager->SiteID && Id.Application == DisManager->ApplicationID)
			{
				SetActorHiddenInGame(true);
				SetActorEnableCollision(false);
			}

			if (!IsHidden())
			{
				DISReceiveComponent->OnReceivedFirePDU.AddDynamic(this, &ATankPawn::OnGhostFired);
			}

			SetTankMesh(Id.Entity);
		}
		
	}
	
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (!bIsLocallyControlled && DISSendComponent)
	{
		DISSendComponent->EntityStatePDUSendingMode = EEntityStateSendingMode::None;
	}

	if (UWorld* World = GetWorld())
	{
		if (ATankGameStateBase* GS = World->GetGameState<ATankGameStateBase>())
		{
			GS->RegisterTank(this);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PostInitializeComponents: No game instance found. Impossible to register tank with id %d"), GetPlayerId());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PostInitializeComponents: No world found. Impossible to register tank with id %d"), GetPlayerId());
	}

	
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::Move(const FInputActionValue& Value)
{
	ForwardInput = Value.Get<float>();
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::Turn(const FInputActionValue& Value)
{
	SteerInput = Value.Get<float>();
}

//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::Fire()
{
	if (!bIsLocallyControlled || !ProjectileClass) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < FireCooldown)
	{
		// cannot fire yet!
		return;
	}
	LastFireTime = Now;

	const int32 EventNumber = NextEventNumber++;

	if (ATankProjectile* SpawnedProjectile = SpawnProjectile(true))
	{
		SpawnedProjectile->EventNumber = EventNumber;
		SendFirePDU(EventNumber, SpawnedProjectile->Speed);
	}

}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::ToggleCamera()
{
	bTopDownView = !bTopDownView;
	ApplyCameraMode();
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::ApplyCameraMode()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	AActor* NewViewTarget = this; // default: tank camera

	if (bTopDownView)
	{
		// look for top down camera in the world only once
		if (!TopDownCameraActor.IsValid())
		{
			TArray<AActor*> Found;
			UGameplayStatics::GetAllActorsWithTag(this, TopDownCameraTag, Found);
			if (Found.Num() > 0)
			{
				TopDownCameraActor = Found[0];
			}
		}

		if (TopDownCameraActor.IsValid())
		{
			NewViewTarget = TopDownCameraActor.Get();
		}
		else
		{
			UE_LOG(LogTemp, Warning,TEXT("TankPawn: No camera with Tag '%s' found in the level. Tanck camera still active."), *TopDownCameraTag.ToString());
		}
	}

	PC->SetViewTargetWithBlend(NewViewTarget, CameraBlendTime);
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::SnapToGround(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Location = GetActorLocation();
	const FVector Start = Location + FVector::UpVector * GroundTraceUp;
	const FVector End = Location - FVector::UpVector * GroundTraceDown;

	// raycast ignores the tank
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TankGroundTrace), /*bTraceComplex=*/false, this);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, GroundTraceChannel, Params))
	{
		// no ground below the tank. No action
		return;
	}

	// Height: follows the terrain with a smooth interpolation 
	const float TargetZ = Hit.ImpactPoint.Z + GroundOffset;
	FVector NewLocation = Location;
	NewLocation.Z = FMath::FInterpTo(Location.Z, TargetZ, DeltaTime, GroundInterpSpeed);
	SetActorLocation(NewLocation, /*bSweep=*/false);

	// Tilt: aligns z-axis to the grond normal keeping the direction of the tank
	if (bAlignToGroundNormal)
	{
		const FRotator TargetRotation =	FRotationMatrix::MakeFromZX(Hit.ImpactNormal, GetActorForwardVector()).Rotator();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, GroundInterpSpeed));
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::SetTankMesh(const int& Id)
{
	if (TankMesh)
	{
		switch (Id)
		{
		case 1:
			if (Player1Mesh)
			{
				TankMesh->SetStaticMesh(Player1Mesh);
			}
			break;

		case 2:
			if (Player2Mesh)
			{
				TankMesh->SetStaticMesh(Player2Mesh);
			}
			break;
		}

	}
}
//---------------------------------------------------------------------------------------------------------------------
void ATankPawn::SetPlayerId(const int NewPlayerId)
{
	PlayerId = NewPlayerId;
}
//---------------------------------------------------------------------------------------------------------------------

int ATankPawn::GetPlayerId()
{
	return PlayerId;
}
//---------------------------------------------------------------------------------------------------------------------

int ATankPawn::GetLocalTankId() const 
{
	return GetLocalEntityID().Entity;
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::OnRemoteHitReceived(float DamageAmount)
{
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);

	UE_LOG(LogTemp, Log, TEXT("OnRemoteHitReceived: Damage Amount: %f. Current health %f. Tanck camera still active."), DamageAmount, CurrentHealth);

	if (CurrentHealth <= 0.f)
	{
		if (ATankGameStateBase* GS = GetWorld()->GetGameState<ATankGameStateBase>())
		{
			GS->ReportLocalTankDestroyed(DISSendComponent->EntityID);
		}
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::OnDetonationReceived(FDetonationPDU DetonationPDU)
{
	// ONLY local tank gets damage
	if (!bIsLocallyControlled || !DISSendComponent)
	{
		return;
	}

	ADISGameManager* DisManager = ADISGameManager::GetDISGameManager(this);
	if (!DisManager)
	{
		return;
	}

	UTankGameInstance* GameInst = Cast<UTankGameInstance>(GetGameInstance());

	if (!GameInst)
	{
		UE_LOG(LogTemp, Error, TEXT("OnDetonationReceived: No game instance found."));
	}

	// gets the complete id of this tank
	const FEntityID LocalId(DisManager->SiteID, DisManager->ApplicationID, DISSendComponent->EntityID);

	if (DetonationPDU.TargetEntityID == LocalId)
	{
		float DamageAmount = 0.f;

		// looks for incoming params
		for (const FArticulationParameters& Param : DetonationPDU.ArticulationParameters)
		{
			// DamageAmount param found (id:9999)
			if (Param.ParameterType == 9999)
			{
				DamageAmount = Param.ParameterValue;
				UE_LOG(LogTemp, Log, TEXT("OnDetonationReceived: Damage Amount received: %f (%f)."), DamageAmount, Param.ParameterValue);
				break;
			}
		}

		if (DamageAmount > 0.f)
		{
			OnRemoteHitReceived(DamageAmount);
		}

		GameInst->RemoveGhostProjectile(DetonationPDU.EventID.EventNumber);
	}
	else if (DetonationPDU.FiringEntityID != LocalId && DetonationPDU.TargetEntityID.Entity==2)
	{
		GameInst->RemoveGhostProjectile(DetonationPDU.EventID.EventNumber);
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::HandleProjectileHit(AActor* HitActor, const FVector& ImpactPoint, float Speed, float Damage, int32 EventNumber)
{
	// who fired the projectile has auth over detonation and sends the info to the client
	ATankPawn* Target = Cast<ATankPawn>(HitActor);
	if (Target && !Target->bIsLocallyControlled && Target->DISReceiveComponent)
	{
		const FEntityID TargetId = Target->DISReceiveComponent->EntityID;
		SendDetonationPDU(Target->DISReceiveComponent->EntityID, ImpactPoint, EventNumber, Speed, Damage);
	}
	else
	{
		// Sending a detonation DPU also when the opposite tank has not been hit
		FEntityID NotTankTargetId = GetLocalEntityID();
		NotTankTargetId.Entity = 2;
		SendDetonationPDU(NotTankTargetId, ImpactPoint, EventNumber, Speed, 0);
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::OnGhostFired(FFirePDU FirePDU)
{
	if (ATankProjectile* GhostBullet = SpawnProjectile(/*bAuthoritative=*/false))
	{
		// Sets the event number on ghost bullet in order to handle its future destruction
		GhostBullet->EventNumber = FirePDU.EventID.EventNumber;
		if (UTankGameInstance* GameInst = Cast<UTankGameInstance>(GetGameInstance()))
		{
			GameInst->AddGhostProjectile(GhostBullet);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("OnGhostFired: No game instance found. Impossible to add ghost projectile for evend id:%d"), GhostBullet->EventNumber);
		}
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsLocallyControlled)
	{
		const FVector MoveDelta = GetActorForwardVector() * ForwardInput * MoveSpeed * DeltaTime;

		FHitResult Hit;
		AddActorWorldOffset(MoveDelta, /*bSweep=*/true, &Hit);

		if (Hit.bBlockingHit)
		{
			// Movement quota remained before the impact
			const FVector Remaining = MoveDelta * (1.f - Hit.Time);

			// Checks slope
			if (Hit.ImpactNormal.Z >= WalkableNormalZ)
			{
				// no obstacle found. we can add movement
				AddActorWorldOffset(Remaining, /*bSweep=*/false);
			}
			else
			{
				// Obstacle found, slide
				const FVector SlideDelta = FVector::VectorPlaneProject(Remaining, Hit.ImpactNormal);
				AddActorWorldOffset(SlideDelta, /*bSweep=*/true);
			}
		}

		const FRotator TurnDelta(0.f, SteerInput * TurnRate * DeltaTime, 0.f);
		AddActorWorldRotation(TurnDelta);

		SnapToGround(DeltaTime);

	}

	// Not local tank will be moved by the DIS system
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// If this is not the locally controlled pawn, we don't bind input actions.
	if (!bIsLocallyControlled)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATankPawn::Move);
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ATankPawn::Move);
		}
		if (TurnAction)
		{
			EnhancedInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ATankPawn::Turn);
			EnhancedInput->BindAction(TurnAction, ETriggerEvent::Completed, this, &ATankPawn::Turn);
		}
		if (FireAction)
		{
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ATankPawn::Fire);
		}
		if (ToggleCameraAction)
		{
			EnhancedInput->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &ATankPawn::ToggleCamera);
		}
	}
}
//---------------------------------------------------------------------------------------------------------------------

ATankProjectile *ATankPawn::SpawnProjectile(bool bAuthoritative)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	if (ATankProjectile* SpawnedProjectile = GetWorld()->SpawnActor<ATankProjectile>(ProjectileClass, Muzzle->GetComponentTransform(), Params))
	{
		SpawnedProjectile->InitProjectile(this, bAuthoritative);
		return SpawnedProjectile;
	}

	return nullptr;
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::SendFirePDU(int32 EventNumber, float ProjectileSpeed)
{
	ADISGameManager* DisManager = ADISGameManager::GetDISGameManager(this);
	AGeoReferencingSystem* GeoRefSys = AGeoReferencingSystem::GetGeoReferencingSystem(this);
	UUDPSubsystem* UdpSubsys = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUDPSubsystem>() : nullptr;
	
	if (!DisManager || !GeoRefSys || !UdpSubsys)
	{
		return;
	}

	const FVector Loc = Muzzle->GetComponentLocation();
	const FVector Dir = Muzzle->GetForwardVector();

	FVector EcefStart, EcefAhead;
	UDIS_BPFL::GetEcefXYZFromUnrealLocation(Loc, GeoRefSys, EcefStart);
	UDIS_BPFL::GetEcefXYZFromUnrealLocation(Loc + Dir * ProjectileSpeed, GeoRefSys, EcefAhead); // spostamento di 1 s

	FFirePDU PDU;
	PDU.ExerciseID = DisManager->ExerciseID;
	PDU.Timestamp = FTimestamp::GenerateRelativeTimestamp(GetWorld()->GetTimeSeconds());
	PDU.FiringEntityID = GetLocalEntityID();
	PDU.EventID = FEventID();
	PDU.EventID.Site = DisManager->SiteID;
	PDU.EventID.Application = DisManager->ApplicationID;
	PDU.EventID.EventNumber = EventNumber;
	PDU.EcefLocation = EcefStart;
	PDU.Velocity = EcefAhead - EcefStart; // m/s in ECEF
	PDU.BurstDescriptor.Quantity = 1;

	UdpSubsys->EmitBytes(UPDUConversions_BPFL::ConvertFirePDUToBytes(PDU));
}
//---------------------------------------------------------------------------------------------------------------------

void ATankPawn::SendDetonationPDU(const FEntityID& TargetId, const FVector& ImpactPoint, int32 EventNumber, float ProjectileSpeed, float DamageAmount)
{
	ADISGameManager* DisManager = ADISGameManager::GetDISGameManager(this);
	AGeoReferencingSystem* GeoRefSys = AGeoReferencingSystem::GetGeoReferencingSystem(this);
	UUDPSubsystem* UdpSubsys = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUDPSubsystem>() : nullptr;

	if (!DisManager || !GeoRefSys || !UdpSubsys)
	{
		return;
	}

	FVector EcefImpact, EcefAhead;
	UDIS_BPFL::GetEcefXYZFromUnrealLocation(ImpactPoint, GeoRefSys, EcefImpact);
	UDIS_BPFL::GetEcefXYZFromUnrealLocation(ImpactPoint + Muzzle->GetForwardVector() * ProjectileSpeed, GeoRefSys, EcefAhead);

	FDetonationPDU PDU;
	PDU.ExerciseID = DisManager->ExerciseID;
	PDU.Timestamp = FTimestamp::GenerateRelativeTimestamp(GetWorld()->GetTimeSeconds());
	PDU.FiringEntityID = GetLocalEntityID();
	PDU.TargetEntityID = TargetId;
	PDU.EventID.Site = DisManager->SiteID;
	PDU.EventID.Application = DisManager->ApplicationID;
	PDU.EventID.EventNumber = EventNumber;
	PDU.EcefLocation = EcefImpact;
	PDU.Velocity = EcefAhead - EcefImpact;
	PDU.BurstDescriptor.Quantity = 1;
	PDU.DetonationResult = EDetonationResult::EntityImpact;

	
	FArticulationParameters DamageParam;

	// Only ArticulatedPart works with ParameterValue according to the documentation
	DamageParam.ParameterTypeDesignator = EVariableParameterRecordType::ArticulatedPart; // 1 = Attached Part (parametro extra allegato)

	DamageParam.ChangeIndicator = 0;
	DamageParam.PartAttachedTo = 0; 
	DamageParam.ParameterType = 9999; // Specific ID used for my DamageAmount param
	DamageParam.ParameterValue = DamageAmount;

	PDU.ArticulationParameters.Add(DamageParam);

	UE_LOG(LogTemp, Log, TEXT("SendDetonationPDU: Damage Amount sent: %f (%f)."), DamageAmount, DamageParam.ParameterValue);


	UdpSubsys->EmitBytes(UPDUConversions_BPFL::ConvertDetonationPDUToBytes(PDU));
}
//---------------------------------------------------------------------------------------------------------------------

FEntityID ATankPawn::GetLocalEntityID() const
{
	ADISGameManager* DisManager = ADISGameManager::GetDISGameManager(const_cast<ATankPawn*>(this));
	if (!DisManager || !DISSendComponent)
	{
		return FEntityID();
	}
	return FEntityID(DisManager->SiteID, DisManager->ApplicationID, DISSendComponent->EntityID);
}
//---------------------------------------------------------------------------------------------------------------------

