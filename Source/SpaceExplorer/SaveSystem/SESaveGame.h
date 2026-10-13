#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SEProgression.h"
#include "SESaveGame.generated.h"

/** Bump when the layout changes so old saves can be migrated or rejected. */
constexpr int32 SE_SAVE_VERSION = 3;

UCLASS()
class SPACEEXPLORER_API USESettingsSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() FSESettings Settings;
};

UCLASS()
class SPACEEXPLORER_API USESaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 SaveVersion = SE_SAVE_VERSION;
    UPROPERTY() FDateTime SavedAt;

    // Galaxy
    UPROPERTY() int32 GalaxySeed = 0;
    UPROPERTY() int32 SystemIndex = 0;
    UPROPERTY() int32 PlanetIndex = 0;

    // Player
    UPROPERTY() FVector PlayerLocation = FVector::ZeroVector;
    UPROPERTY() FRotator PlayerRotation = FRotator::ZeroRotator;
    UPROPERTY() float Health = 100.f;
    UPROPERTY() float Oxygen = 100.f;
    UPROPERTY() float Energy = 100.f;
    UPROPERTY() TMap<FName, int32> Inventory;

    // Ship
    UPROPERTY() bool bHasShip = false;
    UPROPERTY() bool bPilotingShip = false;
    UPROPERTY() FVector ShipLocation = FVector::ZeroVector;
    UPROPERTY() FRotator ShipRotation = FRotator::ZeroRotator;
    UPROPERTY() float ShipFuel = 100.f;
    UPROPERTY() float ShipShield = 100.f;
    UPROPERTY() float ShipHull = 100.f;
    UPROPERTY() float ShipEnergy = 100.f;
    UPROPERTY() TMap<FName, int32> ShipCargo;

    // Progression: credits, research, tech, discoveries, reputation, missions, bases, world time
    UPROPERTY() FSEProgressSave Progress;
};
