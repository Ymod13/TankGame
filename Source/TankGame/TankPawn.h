// Created by Domenico Guaccero - 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TankPawn.generated.h"

class UDISSendComponent;
class UDISReceiveComponent;

// Enhanced Input
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
struct FDetonationPDU;
struct FEntityID;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class TANKGAME_API ATankPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ATankPawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

public:	

	// DIS components

	// True: tank driven by this process (send DIS),
	// Talse: ghost tank driven by the opposite player remotely (receive DIS).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|DIS")
	bool bIsLocallyControlled = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|DIS")
	UDISSendComponent* DISSendComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|DIS")
	UDISReceiveComponent* DISReceiveComponent;

	
	// Gameplay

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Gameplay")
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tank|Gameplay")
	float CurrentHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Gameplay")
	UStaticMesh* Player1Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Gameplay")
	UStaticMesh* Player2Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "Tank|Gameplay")
	TSubclassOf<class AActor> TankExplosionVfxClass;

	
	// Movement

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Movement")
	float MoveSpeed = 300.f; 

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Movement")
	float TurnRate = 90.f; // degrees

	
	// Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	UStaticMeshComponent* TankMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	UCameraComponent* Camera;

	// Firing point on the tank. Set on BP
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	USceneComponent* Muzzle;

	
	// Enhanced Input: assigned in BP_TankPawn

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Input")
	UInputMappingContext* TankMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Input")
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Input")
	UInputAction* TurnAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Input")
	UInputAction* FireAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Input")
	UInputAction* ToggleCameraAction;


	// Camera
	
	// Camera actor tagged with this name will be used for the top-down view.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Camera")
	FName TopDownCameraTag = TEXT("TopDownCamera");

	// Camera Movement blend time
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Camera")
	float CameraBlendTime = 0.4f;

	// Starting view: true = top Down camera, false = tank camera. The player can toggle the view with the ToggleCameraAction input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Camera")
	bool bTopDownView = true;


	// Ground Snapping (no gravity, simple ground check with a trace)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Ground")
	float WalkableNormalZ = 0.7f;

	// Distance above pivot: trace start
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Ground")
	float GroundTraceUp = 200.f;

	// distance below the pivot: trace end
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Ground")
	float GroundTraceDown = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Ground")
	float GroundOffset = 0.f;

	// heigh and slope ground speed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Ground")
	float GroundInterpSpeed = 15.f;

	//  Tilt the tank according to the terrain slope (pitch/roll), maintaining the direction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Ground")
	bool bAlignToGroundNormal = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;


	// Projectile

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Weapon")
	TSubclassOf<class ATankProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Weapon")
	float FireCooldown = 0.5f;


	// Functions

	void Move(const FInputActionValue& Value);
	void Turn(const FInputActionValue& Value);
	void Fire();
	void ToggleCamera();
	void ApplyCameraMode();
	void SnapToGround(float DeltaTime);
	void SetTankMesh(const int& Id);
	void SetPlayerId(const int NewPlayerId);
	int GetPlayerId();
	int GetLocalTankId() const ;

	// called when a detonation DIS arrives
	UFUNCTION(BlueprintCallable, Category = "Tank|DIS")
	void OnRemoteHitReceived(float DamageAmount);

	UFUNCTION()
	void OnDetonationReceived(FDetonationPDU DetonationPDU);

	void HandleProjectileHit(AActor* HitActor, const FVector& ImpactPoint, float Speed, float Damage, int32 EventNumber);

	UFUNCTION()
	void OnGhostFired(FFirePDU FirePDU);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:

	ATankProjectile *SpawnProjectile(bool bAuthoritative);
	void SendFirePDU(int32 EventNumber, float ProjectileSpeed);
	void SendDetonationPDU(const FEntityID& TargetId, const FVector& ImpactPoint, int32 EventNumber, float ProjectileSpeed, float DamageAmount);
	FEntityID GetLocalEntityID() const;

	int32 NextEventNumber = 1;
	float LastFireTime = -1000.f;
	
	TWeakObjectPtr<AActor> TopDownCameraActor;

	float ForwardInput = 0.f;
	float SteerInput = 0.f;
	int PlayerId = 0;

};
