#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SESaveSubsystem.generated.h"

class ASEPlayerCharacter;
class ASEShip;
class USESaveGame;

UCLASS()
class SPACEEXPLORER_API USESaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Save") bool SaveToSlot(const FString& SlotName);
    /** Loads a slot. If the save is on another planet the level reloads there first and the load completes afterwards. */
    UFUNCTION(BlueprintCallable, Category="Save") bool LoadFromSlot(const FString& SlotName);
    UFUNCTION(BlueprintPure, Category="Save") bool SlotExists(const FString& SlotName) const;
    UFUNCTION(BlueprintCallable, Category="Save") bool DeleteSlot(const FString& SlotName);
    /** Keeps inventory, vitals and ship state across the level reload, then travels. */
    bool BeginTravel(int32 System, int32 Planet);
    /** Called by the game mode after a reload to finish a pending load or travel. */
    void CompletePendingLoad();

private:
    ASEPlayerCharacter* GetPlayer() const;
    ASEShip* FindShip() const;
    bool FillSave(USESaveGame* Save) const;
    bool ApplySave(USESaveGame* Save, bool bRestorePositions);

    UPROPERTY() TObjectPtr<USESaveGame> Transit;
};
