// Created by Domenico Guaccero - 2026


#include "TankGameStateBase.h"
#include "TankGameInstance.h"
#include "TankPawn.h"

#include "UDPSubsystem.h"
#include "PDUProcessor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "DISGameManager.h"
#include "TankGameModeBase.h"

ATankGameStateBase::ATankGameStateBase()
{
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::ReportLocalTankDestroyed(int32 MyEntityID)
{
	// Autorità: solo chi muore scrive il risultato del round. Calcoliamo subito il nuovo
	// stato in locale, poi lo annunciamo. L'EntityID 1 segna il punto al giocatore 2 e
	// viceversa (vince chi NON è morto).
	FTankRoundResultPacket Packet;
	Packet.LoserEntityID = static_cast<uint8>(MyEntityID);
	Packet.NewRoundNumber = static_cast<uint8>(CurrentRound + 1);
	Packet.ScorePlayer1 = static_cast<uint8>(ScorePlayer1 + (MyEntityID == 2 ? 1 : 0));
	Packet.ScorePlayer2 = static_cast<uint8>(ScorePlayer2 + (MyEntityID == 1 ? 1 : 0));

	ApplyRoundResult(Packet);

	// Rinvio per qualche centinaio di ms: l'UDP può perdere il pacchetto, e questo è
	// l'unico fatto che l'altro lato non potrebbe altrimenti scoprire da solo.
	PendingPacket = Packet;
	ResendCount = 0;
	SendPacketViaSignalPDU(Packet);

	GetWorldTimerManager().SetTimer(
		ResendTimerHandle,
		FTimerDelegate::CreateLambda([this]()
			{
				if (ResendCount >= MaxResends)
				{
					GetWorldTimerManager().ClearTimer(ResendTimerHandle);
					return;
				}
				++ResendCount;
				SendPacketViaSignalPDU(PendingPacket);
			}),
		ResendInterval,
		/*bLoop=*/true);
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::ResetGameData()
{
	FTankRoundResultPacket Packet; 
	Packet.LoserEntityID = 0;
	Packet.NewRoundNumber = 1;
	Packet.ScorePlayer1 = 0;
	Packet.ScorePlayer2 = 0;

	ApplyRoundResult(Packet);

	PendingPacket = Packet;
	ResendCount = 0;
	SendPacketViaSignalPDU(Packet);

	GetWorldTimerManager().SetTimer(
		ResendTimerHandle,
		FTimerDelegate::CreateLambda([this]()
			{
				if (ResendCount >= MaxResends)
				{
					GetWorldTimerManager().ClearTimer(ResendTimerHandle);
					return;
				}
				++ResendCount;
				SendPacketViaSignalPDU(PendingPacket);
			}),
		ResendInterval,
		/*bLoop=*/true);
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::RegisterTank(ATankPawn* NewTank)
{
	for (APlayerState* PS : PlayerArray)   // dentro un metodo di AGameStateBase
	{
		if (!PS) continue;

		if (ATankPawn* Tank = PS->GetPawn<ATankPawn>())
		{
			CurrentPlayerId = Tank->GetPlayerId();
			OppositePlayerId = CurrentPlayerId == 1 ? 2 : 1;
		}
	}

	ADISGameManager *LevelDISManager = ADISGameManager::GetDISGameManager(GetWorld());
	
	if (!LevelDISManager) {
		UE_LOG(LogTemp, Warning, TEXT("RegisterTank: Level DIS manager not found!"));
	}

	const FEntityID& NewTankId = NewTank->DISReceiveComponent->EntityID;
	UE_LOG(LogTemp, Log, TEXT("Local DisManManager	->  Site: %d - App: %d - Exc: %d"), LevelDISManager->SiteID, LevelDISManager->ApplicationID, LevelDISManager->ExerciseID);
	UE_LOG(LogTemp, Log, TEXT("NewTank Info			->  Site: %d - App: %d - Entity: %d"), NewTankId.Site, NewTankId.Application, NewTankId.Entity);

	if (!NewTank->bIsLocallyControlled) 
	{
		// Setting the right id to remote tank
		NewTank->DISSendComponent->EntityID = OppositePlayerId;
	}

	// tanks with application id == 0 seems to be locally controlled.
	// the echo comes with the same app lication id of the dis manager but is not locally controlled
	bool bCanAdd = false;
	if (NewTank->bIsLocallyControlled || NewTankId.Application != LevelDISManager->ApplicationID)
	{
		bCanAdd = true;
	}

	if (bCanAdd) 
	{
		RegistredTanks.AddUnique(NewTank);

		UE_LOG(LogTemp, Warning, TEXT("RegisterTank: Tank Registered. Player Id: %d, Locally Controlled %d"), NewTank->GetLocalTankId(), NewTank->bIsLocallyControlled);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RegisterTank: Tank Already Registered (this is the echo, we are not registering it). Player Id: %d, Locally Controlled %d"), NewTank->GetLocalTankId(), NewTank->bIsLocallyControlled);
	}

}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPDUProcessor* Processor = GI->GetSubsystem<UPDUProcessor>())
		{
			Processor->OnSignalPDUProcessed.AddDynamic(this, &ATankGameStateBase::HandleSignalPDUProcessed);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ATankGameStateBase: UPDUProcessor subsystem not found."));
		}
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPDUProcessor* Processor = GI->GetSubsystem<UPDUProcessor>())
		{
			Processor->OnSignalPDUProcessed.RemoveDynamic(this, &ATankGameStateBase::HandleSignalPDUProcessed);
		}
	}

	GetWorldTimerManager().ClearTimer(ResendTimerHandle);

	Super::EndPlay(EndPlayReason);
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::HandleSignalPDUProcessed(FSignalPDU SignalPDUIn)
{
	// Questo delegate si attiva per QUALSIASI Signal PDU ricevuta sul canale DIS, quindi
	// il controllo sul MagicByte è quello che scarta tutto ciò che non è nostro.
	FTankRoundResultPacket Packet;
	if (!FTankRoundResultPacket::FromBytes(SignalPDUIn.Data, Packet))
	{
		return;
	}

	ApplyRoundResult(Packet);
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::ApplyRoundResult(const FTankRoundResultPacket& Packet)
{
	// Idempotente: un rinvio dello stesso round, o l'eco del nostro stesso pacchetto
	// (loopback abilitato per test sullo stesso PC), non lo applica due volte.
	if (Packet.NewRoundNumber <= LastAppliedRoundNumber && Packet.LoserEntityID > 0)
	{
		return;
	}

	LastAppliedRoundNumber = Packet.NewRoundNumber;

	ScorePlayer1 = Packet.ScorePlayer1;
	ScorePlayer2 = Packet.ScorePlayer2;
	CurrentRound = Packet.NewRoundNumber;

	OnScoreChanged.Broadcast();

	if (Packet.LoserEntityID <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyRoundResult: Reset Game data!"));
		bIsMatchOver = true;
		InfoMessage = "Find and shoot your opponent!";
		RespawnPlayer(GetLocalTank());
	}
	else
	{
		// The match is in course
		const int32 Winner = (Packet.LoserEntityID == 1) ? 2 : 1;
		const int32 WinnerScore = (Winner == 1) ? ScorePlayer1 : ScorePlayer2;

		if (WinnerScore >= RoundsToWin)
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyRoundResult: Match OVER!"));
			OnMatchOver.Broadcast(Winner);
			bIsMatchOver = true;
			InfoMessage = "The winner is PLAYER " + Winner;

			ApplyTankDestruction(Packet.LoserEntityID, false);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("ApplyRoundResult: Match is still Live!"));
			bIsMatchOver = false;
			InfoMessage = "Find and shoot your opponent!";
			ApplyTankDestruction(Packet.LoserEntityID, true);
		}

		
	}
	
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::SendPacketViaSignalPDU(const FTankRoundResultPacket& Packet)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("SendPacketViaSignalPDU: Game Instance not found, can't send round result."));
		return;
	}

	UUDPSubsystem* UDP = GI->GetSubsystem<UUDPSubsystem>();
	if (!UDP)
	{
		UE_LOG(LogTemp, Error, TEXT("SendPacketViaSignalPDU: UUDPSubsystem not found, can't send round result."));
		return;
	}

	// Il campo Data di una Signal PDU è pensato per contenuti applicativi arbitrari.
	// EncodingScheme/TDLType/SampleRate/Samples restano ai valori di default: non ci
	// servono, i due lati si riconoscono solo tramite il MagicByte dentro Data.
	FSignalPDU Signal;
	Signal.Data = Packet.ToBytes();

	// Esce sullo stesso/gli stessi send socket DIS già aperti (stesse porte incrociate
	// già configurate per Entity State/Fire/Detonation PDU), nessun canale aggiuntivo.
	UDP->EmitBytes(Signal.ToBytes());

	UE_LOG(LogTemp, Log, TEXT("SendPacketViaSignalPDU: Packet SENT!"));
}
//---------------------------------------------------------------------------------------------------------------------

ATankPawn* ATankGameStateBase::GetLooserTank(const int32& InPlayerId)
{
	ATankPawn* FoundTank = nullptr;

	for (ATankPawn* Tank : RegistredTanks)
	{
		if (Tank->GetLocalTankId() == InPlayerId)
		{
			FoundTank = Tank;
			UE_LOG(LogTemp, Log, TEXT("GetLooserTank: Tank found! Id: %d"), InPlayerId);
		}
	}

	if (!FoundTank)
	{
		UE_LOG(LogTemp, Error, TEXT("GetLooserTank: NO Tank found with Id: %d"), InPlayerId);
	}

	return FoundTank;
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::ApplyTankDestruction(const int32& InLosingPlayerId, const bool& bRespawnLooser)
{
	if (ATankPawn* TankToDestroy = GetLooserTank(InLosingPlayerId))
	{
		if (IsValid(TankToDestroy->TankExplosionVfxClass))
		{
			//SetActorHiddenInGame(true);

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.Owner = this;
			// Tank Cosmetic explosion
			GetWorld()->SpawnActor<AActor>(TankToDestroy->TankExplosionVfxClass, TankToDestroy->GetActorTransform(), Params);

			UE_LOG(LogTemp, Log, TEXT("ApplyTankDestruction: Applied destruction VFX to Tank (Id: %d)!"), InLosingPlayerId);

			if (bRespawnLooser && TankToDestroy->bIsLocallyControlled)
			{
				RespawnPlayer(TankToDestroy);
			}
		}
	}
	else 
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyTankDestruction:Impossible to apply tank destruction and respawn for Tank with Id: %d"), InLosingPlayerId);
	}
}
//---------------------------------------------------------------------------------------------------------------------

void ATankGameStateBase::RespawnPlayer(ATankPawn* PlayerToRespawn)
{
	if (!PlayerToRespawn)
	{
		UE_LOG(LogTemp, Error, TEXT("RespawnPlayer: Impossible to respawn player: tank not found!"));
		return;
	}
	if (PlayerToRespawn->bIsLocallyControlled)
	{
		if (ATankGameModeBase* GM = GetWorld()->GetAuthGameMode<ATankGameModeBase>())
		{
			if (AActor* RespawnPoint = GM->PickRandomStartPoint())
			{
				PlayerToRespawn->SetActorLocationAndRotation(
					RespawnPoint->GetActorLocation(),
					RespawnPoint->GetActorRotation(),
					false,
					nullptr,
					ETeleportType::TeleportPhysics);

				if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(PlayerToRespawn->GetRootComponent()))
				{
					Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
					Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				}
			}
		}
	}
}
//---------------------------------------------------------------------------------------------------------------------

ATankPawn* ATankGameStateBase::GetLocalTank()
{
	for (ATankPawn* Tank : RegistredTanks)
	{
		if (Tank->bIsLocallyControlled)
		{
			return Tank;
		}
	}
	return nullptr;
}
//---------------------------------------------------------------------------------------------------------------------
