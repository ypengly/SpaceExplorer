#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SEInteractable.h"
#include "SEProgression.h"
#include "SEWorldObjects.generated.h"

class ASEPlayerCharacter;
class UStaticMeshComponent;
class ASENPC;
class ASETerrainStreamer;

/** Mineable resource deposit. Mined with the multitool; depleted deposits stay gone (saved). */
UCLASS()
class SPACEEXPLORER_API ASEResourceNode : public AActor, public ISEScannable
{
    GENERATED_BODY()
public:
    ASEResourceNode();
    void InitNode(FName Item, int32 Amount, int32 Key, const FLinearColor& Color, float Scale);
    int32 Mine(ASEPlayerCharacter* Player, int32 Amount);

    virtual FName GetDiscoveryId() const override { return FName(*(TEXT("resource:") + ItemId.ToString())); }
    virtual FString GetScanName() const override;
    virtual FString GetScanInfo() const override { return FString::Printf(TEXT("%d units remaining. Mine with the multitool (left mouse)."), Remaining); }
    virtual int32 GetScanReward() const override { return 6; }
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    FName ItemId; int32 Remaining = 0; int32 MaxAmount = 1; int32 NodeKey = 0; float BaseScale = 1.f;
};

/** Ancient alien structure. Examine once (F) for an artifact and research. */
UCLASS()
class SPACEEXPLORER_API ASERuin : public AActor, public ISEInteractable, public ISEScannable
{
    GENERATED_BODY()
public:
    ASERuin();
    void InitRuin(int32 Key);
    virtual void Interact(ASEPlayerCharacter* Interactor) override;
    virtual FText GetInteractPrompt() const override;
    virtual FName GetDiscoveryId() const override { return FName(*FString::Printf(TEXT("structure:ruin_%d"), RuinKey)); }
    virtual FString GetScanName() const override { return TEXT("Alien Ruin"); }
    virtual FString GetScanInfo() const override { return TEXT("Weathered monoliths of an unknown builder. Something is inside."); }
    virtual int32 GetScanReward() const override { return 30; }
private:
    int32 RuinKey = 0;
};

/** Player-built base piece. Habitat, Oxygen Beacon, Solar Generator and Fabricator have gameplay effects. F dismantles. */
UCLASS()
class SPACEEXPLORER_API ASEBaseStructure : public AActor, public ISEInteractable
{
    GENERATED_BODY()
public:
    ASEBaseStructure();
    void InitStructure(FName InType, bool bGhost);
    void SetGhostColor(bool bAffordable);
    FName GetType() const { return Type; }
    virtual void Interact(ASEPlayerCharacter* Interactor) override;
    virtual FText GetInteractPrompt() const override;

    /** Destroys and re-spawns every saved structure that belongs to the current planet. */
    static void SpawnAllBuilt(UWorld* World);
    struct FZone { bool bShelter = false; bool bFabricator = false; bool bSolar = false; bool bHeal = false; };
    static FZone QueryZone(UWorld* World, const FVector& Loc);
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    FName Type;
    bool bIsGhost = false;
};

/** Short-lived physics debris for explosions. */
UCLASS()
class SPACEEXPLORER_API ASEDebris : public AActor
{
    GENERATED_BODY()
public:
    ASEDebris();
    void Launch(const FLinearColor& Color, const FVector& Impulse);
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
};

/** Settlement of a faction: buildings, a plaza and 3-4 NPCs on schedules. */
UCLASS()
class SPACEEXPLORER_API ASESettlement : public AActor, public ISEScannable
{
    GENERATED_BODY()
public:
    ASESettlement();
    void InitSettlement(int32 Seed, FName Faction, const FVector& Center, ASETerrainStreamer* Terrain, int32 PlanetSeed, const TArray<FName>& SpeciesIds, int32 Sys, int32 Planet);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual FName GetDiscoveryId() const override { return FName(*(TEXT("settlement:") + SettlementName)); }
    virtual FString GetScanName() const override { return SettlementName; }
    virtual FString GetScanInfo() const override { return FString::Printf(TEXT("Settlement of the %s."), *FactionName); }
    virtual int32 GetScanReward() const override { return 25; }
private:
    FString SettlementName, FactionName;
    TArray<TWeakObjectPtr<ASENPC>> Residents;
};
