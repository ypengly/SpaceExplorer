#pragma once
#include "CoreMinimal.h"
#include "SEWalker.h"
#include "SEInteractable.h"
#include "SEGalaxyTypes.h"
#include "SECreature.generated.h"

class USEProgression;

struct FSESpecies
{
    FName Id;
    FString Name;
    FString Desc;
    /** 0 herbivore, 1 carnivore */
    int32 Diet = 0;
    float Size = 1.f, Speed = 300.f, Health = 40.f, Damage = 5.f;
    bool bPack = false, bNocturnal = false, bTerritorial = false, bSkittish = true;
    FLinearColor Color = FLinearColor::Gray;
    int32 ScanReward = 15;
};

namespace SECreatureGen
{
    /** Deterministic species list for a planet. */
    SPACEEXPLORER_API TArray<FSESpecies> Generate(const FSEPlanetData& Planet);
}

/** Alien animal: idle, wander, feed, hunt, flee, defend territory, sleep (day/night), pack following and reproduction. */
UCLASS()
class SPACEEXPLORER_API ASECreature : public ASEWalker, public ISEScannable
{
    GENERATED_BODY()
public:
    void InitCreature(const FSESpecies& S, ASECreature* InLeader, const FVector& InHome, bool bJuvenile = false);
    void TakeToolDamage(float Amount, AActor* Source);
    bool IsDead() const { return bDead; }
    const FSESpecies& GetSpecies() const { return Species; }
    void ResetReproduction() { ReproTimer = 300.f; }
    static int32 CountSpecies(UWorld* World, FName SpeciesId);

    virtual FName GetDiscoveryId() const override { return FName(*(TEXT("creature:") + Species.Id.ToString())); }
    virtual FString GetScanName() const override { return Species.Name; }
    virtual FString GetScanInfo() const override { return Species.Desc; }
    virtual int32 GetScanReward() const override { return Species.ScanReward; }

protected:
    virtual void Think(float Dt) override;

private:
    enum class EState : uint8 { Idle, Wander, Feed, Hunt, Flee, Defend, Sleep };
    void Die(AActor* Killer);
    void Sense();
    void SetState(EState S, float Duration) { State = S; StateTimer = Duration; bArrived = false; }
    FVector RandomPointAround(const FVector& Center, float Radius) const;

    FSESpecies Species;
    EState State = EState::Idle;
    FVector Home = FVector::ZeroVector;
    FVector MoveTarget = FVector::ZeroVector;
    TWeakObjectPtr<ASECreature> Leader;
    TWeakObjectPtr<AActor> Target;
    TWeakObjectPtr<AActor> Threat;
    float Health = 40.f, Hunger = 0.2f, StateTimer = 2.f, AttackCooldown = 0.f, SenseTimer = 0.f, ReproTimer = 120.f, Age = 999.f;
    bool bDead = false, bArrived = false;
    UPROPERTY() TObjectPtr<USEProgression> Progression;
};
