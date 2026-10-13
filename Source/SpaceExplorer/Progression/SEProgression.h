#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SEProgression.generated.h"

USTRUCT()
struct FSEMission
{
    GENERATED_BODY()
    UPROPERTY() FName Id;
    UPROPERTY() FString Title;
    UPROPERTY() FString Desc;
    /** 0 collect, 1 scan, 2 hunt, 3 explore */
    UPROPERTY() int32 Type = 0;
    UPROPERTY() FName Target;
    UPROPERTY() int32 Required = 1;
    UPROPERTY() int32 Progress = 0;
    UPROPERTY() FVector Location = FVector::ZeroVector;
    UPROPERTY() int32 SystemIdx = 0;
    UPROPERTY() int32 PlanetIdx = 0;
    UPROPERTY() FName Faction;
    UPROPERTY() FString GiverName;
    UPROPERTY() int32 RewardCredits = 0;
    UPROPERTY() int32 RewardRep = 0;
    UPROPERTY() int32 RewardResearch = 0;
    bool IsComplete() const { return Progress >= Required; }
};

USTRUCT()
struct FSEBuiltStructure
{
    GENERATED_BODY()
    UPROPERTY() FName Type;
    UPROPERTY() FVector Location = FVector::ZeroVector;
    UPROPERTY() float Yaw = 0.f;
    UPROPERTY() int32 SystemIdx = 0;
    UPROPERTY() int32 PlanetIdx = 0;
};

USTRUCT()
struct FSESettings
{
    GENERATED_BODY()
    UPROPERTY() float MouseSensitivity = 1.f;
    UPROPERTY() float FOV = 90.f;
    UPROPERTY() int32 ViewRadius = 8;
    UPROPERTY() bool bInvertY = false;
    UPROPERTY() bool bAutosave = true;
};

/** Everything that persists about the player's progress. Saved as one block. */
USTRUCT()
struct FSEProgressSave
{
    GENERATED_BODY()
    UPROPERTY() int32 Credits = 100;
    UPROPERTY() int32 ResearchPoints = 0;
    UPROPERTY() int32 XP = 0;
    UPROPERTY() float WorldHours = 9.f;
    UPROPERTY() TArray<FName> UnlockedTech;
    UPROPERTY() TMap<FName, FString> Discoveries;     // id -> "Name - info"
    UPROPERTY() TMap<FName, int32> Reputation;        // faction -> -100..100
    UPROPERTY() TArray<FSEMission> ActiveMissions;
    UPROPERTY() TArray<FName> CompletedMissions;
    UPROPERTY() TArray<FSEBuiltStructure> Built;
    UPROPERTY() TArray<int32> Depleted;               // resource nodes / ruins already used
    UPROPERTY() TMap<FName, int32> Kills;
};

class ASEPlayerCharacter;
struct FSEPlanetData;

UCLASS()
class SPACEEXPLORER_API USEProgression : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    FSEProgressSave Data;
    FSESettings Settings;
    /** Set before a level reload (travel / cross-planet load) so the main menu is not shown again. */
    bool bSkipMainMenu = false;

    struct FNotice { FString Text; double Time; };
    TArray<FNotice> Notices;
    void PushNotice(const FString& Text);

    // economy / progression
    int32 GetLevel() const { return 1 + Data.XP / 100; }
    void AddXP(int32 Amount);
    void AddCredits(int32 Amount) { Data.Credits += Amount; }
    bool SpendCredits(int32 Amount);
    void AddResearch(int32 Amount) { Data.ResearchPoints += Amount; }
    bool HasTech(FName Id) const { return Id.IsNone() || Data.UnlockedTech.Contains(Id); }
    bool CanUnlock(FName Id, FString& WhyNot) const;
    bool UnlockTech(FName Id);
    int32 TechLevel(const FString& Prefix) const;   // e.g. "engine_" -> count of unlocked engine_* techs
    int32 GetRep(FName Faction) const { const int32* R = Data.Reputation.Find(Faction); return R ? *R : 0; }
    void AddRep(FName Faction, int32 Delta);
    /** Returns true if this is a new discovery. */
    bool AddDiscovery(FName Id, const FString& Name, const FString& Info, int32 Research);

    // world state
    bool IsDepleted(int32 Key) const { return Data.Depleted.Contains(Key); }
    void MarkDepleted(int32 Key) { Data.Depleted.AddUnique(Key); }

    // missions
    bool AcceptMission(const FSEMission& M);
    void AbandonMission(int32 Index);
    bool IsMissionTaken(FName Id) const;
    void ProgressMissions(int32 Type, FName Target, int32 Amount);
    void CheckLocation(const FVector& Loc, int32 System, int32 Planet);
    static TArray<FSEMission> GenerateMissions(int32 Seed, FName Faction, const FString& Giver, int32 Sys, int32 Planet, const FVector& Origin, const TArray<FName>& SpeciesIds);

    void ResetForNewGame();
    void SaveSettings() const;
    void LoadSettings();
private:
    void CheckCompletions();
};
