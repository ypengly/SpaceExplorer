#pragma once
#include "CoreMinimal.h"
#include "Creatures/SEWalker.h"
#include "SEInteractable.h"
#include "SEProgression.h"
#include "SENPC.generated.h"

/** Alien civilian: name, job, faction, daily schedule, dialogue, trading, missions, and reactions to the player. */
UCLASS()
class SPACEEXPLORER_API ASENPC : public ASEWalker, public ISEInteractable
{
    GENERATED_BODY()
public:
    FString NPCName;
    FString Job;
    FName Faction;
    int32 PlanetSeed = 0;
    TArray<FName> Stock;
    TArray<FSEMission> Offers;

    void InitNPC(int32 Seed, const FString& InJob, FName InFaction, const FVector& InHome, const FVector& InWork, const FVector& InSocial,
                 int32 InPlanetSeed, const TArray<FName>& SpeciesIds, int32 Sys, int32 Planet);
    FString GetActivity() const;
    FString GetGreeting() const;
    const TArray<FString>& GetTalkLines() const { return Lines; }
    void TakeToolDamage(float Amount, AActor* Source);

    virtual void Interact(ASEPlayerCharacter* Interactor) override;
    virtual FText GetInteractPrompt() const override;

protected:
    virtual void Think(float Dt) override;

private:
    FVector HomePos, WorkPos, SocialPos;
    TArray<FString> Lines;
    float Health = 60.f;
    UPROPERTY() TObjectPtr<USEProgression> Progression;
};
