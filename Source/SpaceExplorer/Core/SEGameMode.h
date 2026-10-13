#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SEGameMode.generated.h"

class ASETerrainStreamer;
class ASEShip;
class ASEPopulator;
class ASEEnvironment;

/** Builds the current planet (terrain, sky, populator), spawns the ship and saved bases, and runs autosave. */
UCLASS()
class SPACEEXPLORER_API ASEGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASEGameMode();
    virtual void PreInitializeComponents() override;
    virtual void StartPlay() override;

    UPROPERTY(EditAnywhere, Category="Save") float AutosaveSeconds = 180.f;

private:
    UPROPERTY() TObjectPtr<ASETerrainStreamer> Streamer;
    UPROPERTY() TObjectPtr<ASEShip> PlayerShip;
    UPROPERTY() TObjectPtr<ASEPopulator> Populator;
    UPROPERTY() TObjectPtr<ASEEnvironment> Environment;
    FTimerHandle PendingLoadTimer, AutosaveTimer;
};
