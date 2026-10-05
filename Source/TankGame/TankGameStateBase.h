// Created by Domenico Guaccero - 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PDUs/RadioCommunicationsFamily/GRILL_SignalPDU.h"
#include "TankGameStateBase.generated.h"

class ATankPawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTankScoreChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTankMatchOver, int32, WinningEntityID);

/**
 * Pacchetto minimo per annunciare la fine di un round. Viaggia DENTRO il campo Data di
 * una FSignalPDU (vedi GRILL_SignalPDU.h): non è un uso "audio/radio" del campo, ma è
 * esattamente il tipo di impiego per cui quel campo esiste (dati applicativi arbitrari).
 * Il MagicByte distingue i nostri pacchetti da un'eventuale Signal PDU usata per altro.
 */
struct FTankRoundResultPacket
{
	static constexpr uint8 MagicByte = 0x54; // 'T'

	uint8 Magic = MagicByte;
	uint8 LoserEntityID = 0;
	uint8 NewRoundNumber = 0;
	uint8 ScorePlayer1 = 0;
	uint8 ScorePlayer2 = 0;

	TArray<uint8> ToBytes() const
	{
		return { Magic, LoserEntityID, NewRoundNumber, ScorePlayer1, ScorePlayer2 };
	}

	static bool FromBytes(const TArray<uint8>& Bytes, FTankRoundResultPacket& Out)
	{
		if (Bytes.Num() < 5 || Bytes[0] != MagicByte)
		{
			return false;
		}
		Out.Magic = Bytes[0];
		Out.LoserEntityID = Bytes[1];
		Out.NewRoundNumber = Bytes[2];
		Out.ScorePlayer1 = Bytes[3];
		Out.ScorePlayer2 = Bytes[4];
		return true;
	}
};

/**
 * 
 */
UCLASS()
class TANKGAME_API ATankGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:

	ATankGameStateBase();
	// --- Dati leggibili dalla UI ---

	UPROPERTY(BlueprintReadOnly, Category = "Tank|Score")
	int32 ScorePlayer1 = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tank|Score")
	int32 ScorePlayer2 = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tank|Score")
	int32 CurrentRound = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Score")
	int32 RoundsToWin = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Score")
	bool bIsMatchOver = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Score")
	FString InfoMessage = "Find and shoot your opponent!";

	// La UI si aggancia a questo invece di fare polling sui valori sopra ogni frame.
	UPROPERTY(BlueprintAssignable, Category = "Tank|Score")
	FTankScoreChanged OnScoreChanged;

	// Si attiva quando un giocatore raggiunge RoundsToWin. Parametro: EntityID del vincitore.
	UPROPERTY(BlueprintAssignable, Category = "Tank|Score")
	FTankMatchOver OnMatchOver;

	/**
	 * Da chiamare SOLO dal carro locale (bIsLocallyControlled == true) quando la SUA vita
	 * arriva a zero. MyEntityID è l'EntityID del carro appena morto (il "perdente" del round).
	 * Aggiorna subito lo stato in locale e lo annuncia all'altro processo via Signal PDU.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tank|Score")
	void ReportLocalTankDestroyed(int32 MyEntityID);

	UFUNCTION(BlueprintCallable, Category = "Tank|Score")
	void ResetGameData();

	void RegisterTank(ATankPawn *NewTank);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	// Bound a UPDUProcessor::OnSignalPDUProcessed: si attiva per OGNI Signal PDU ricevuta,
	// quindi il primo controllo è sempre il MagicByte per scartare quelle non nostre.
	UFUNCTION()
	void HandleSignalPDUProcessed(FSignalPDU SignalPDUIn);

	void ApplyRoundResult(const FTankRoundResultPacket& Packet);
	void SendPacketViaSignalPDU(const FTankRoundResultPacket& Packet);

	ATankPawn* GetLooserTank(const int32& InPlayerId);
	void ApplyTankDestruction(const int32& InLosingPlayerId, const bool &bRespawnLooser);
	void RespawnPlayer(ATankPawn* PlayerToRespawn);
	ATankPawn* GetLocalTank();

	// Rinvio del pacchetto in uscita (l'UDP può perderlo). Idempotente lato ricezione
	// grazie al controllo su LastAppliedRoundNumber.
	FTimerHandle ResendTimerHandle;
	FTankRoundResultPacket PendingPacket;
	int32 ResendCount = 0;
	static constexpr int32 MaxResends = 5;
	static constexpr float ResendInterval = 0.3f;

	// Evita di riapplicare due volte lo stesso risultato (round duplicato per rinvio,
	// o eco del nostro stesso pacchetto se la ricezione DIS ha loopback attivo).
	int32 LastAppliedRoundNumber = 0;

	TArray<ATankPawn*> RegistredTanks;

	int CurrentPlayerId = 0;
	int OppositePlayerId = 0;
};
