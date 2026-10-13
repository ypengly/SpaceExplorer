#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SEGalaxyTypes.h"
#include "SEGalaxySubsystem.generated.h"

/**
 * Deterministic procedural galaxy. Nothing is stored: every system is rebuilt from
 * (GalaxySeed, SystemIndex), so a galaxy of thousands of systems costs no memory or save space.
 * Also tracks which planet the player is currently on; the level is reloaded to travel.
 */
UCLASS()
class SPACEEXPLORER_API USEGalaxySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    static constexpr int32 SystemCount = 2000;
    static constexpr float GalaxyRadiusLY = 40000.f;

    UPROPERTY(BlueprintReadWrite, Category="Galaxy") int32 GalaxySeed = 0xC0FFEE;
    UPROPERTY(BlueprintReadOnly, Category="Galaxy") int32 CurrentSystem = 0;
    UPROPERTY(BlueprintReadOnly, Category="Galaxy") int32 CurrentPlanet = 0;
    /** Save slot to load once the next level has started (set when loading a save from another planet). */
    FString PendingLoadSlot;

    UFUNCTION(BlueprintPure, Category="Galaxy") FVector GetSystemPosition(int32 Index) const;
    UFUNCTION(BlueprintCallable, Category="Galaxy") FSEStarSystemData BuildSystem(int32 Index) const;
    UFUNCTION(BlueprintCallable, Category="Galaxy") TArray<int32> GetNearestSystems(int32 FromIndex, int32 Count) const;
    UFUNCTION(BlueprintPure, Category="Galaxy") FSEStarSystemData GetCurrentSystem() const { return BuildSystem(CurrentSystem); }
    UFUNCTION(BlueprintPure, Category="Galaxy") FSEPlanetData GetCurrentPlanet() const;

    /** Sets the destination and reloads the level; the world generates the new planet on start. */
    UFUNCTION(BlueprintCallable, Category="Galaxy") bool TravelTo(int32 System, int32 Planet);
    /** Changes destination without reloading (used by the save system). */
    void SetCurrent(int32 System, int32 Planet);
};
