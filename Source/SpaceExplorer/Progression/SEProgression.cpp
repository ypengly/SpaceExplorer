#include "SEProgression.h"
#include "SEGameData.h"
#include "SESaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"

void USEProgression::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<USEGameData>();
    LoadSettings();
}

void USEProgression::PushNotice(const FString& Text)
{
    Notices.Add({ Text, FPlatformTime::Seconds() });
    if (Notices.Num() > 8) Notices.RemoveAt(0);
}

void USEProgression::AddXP(int32 Amount)
{
    const int32 Before = GetLevel();
    Data.XP += Amount;
    if (GetLevel() > Before) PushNotice(FString::Printf(TEXT("Level up! You are now level %d"), GetLevel()));
}

bool USEProgression::SpendCredits(int32 Amount)
{
    if (Data.Credits < Amount) return false;
    Data.Credits -= Amount;
    return true;
}

bool USEProgression::CanUnlock(FName Id, FString& WhyNot) const
{
    USEGameData* GD = GetGameInstance()->GetSubsystem<USEGameData>();
    const FSETechNode* T = GD ? GD->FindTech(Id) : nullptr;
    if (!T) { WhyNot = TEXT("Unknown technology"); return false; }
    if (HasTech(Id)) { WhyNot = TEXT("Already unlocked"); return false; }
    for (FName P : T->Prereqs) { if (!HasTech(P)) { WhyNot = TEXT("Requires ") + GD->FindTech(P)->Name; return false; } }
    if (Data.ResearchPoints < T->Cost) { WhyNot = FString::Printf(TEXT("Needs %d research"), T->Cost); return false; }
    return true;
}

bool USEProgression::UnlockTech(FName Id)
{
    FString Why;
    if (!CanUnlock(Id, Why)) { PushNotice(Why); return false; }
    const FSETechNode* T = GetGameInstance()->GetSubsystem<USEGameData>()->FindTech(Id);
    Data.ResearchPoints -= T->Cost;
    Data.UnlockedTech.Add(Id);
    PushNotice(TEXT("Researched: ") + T->Name);
    return true;
}

int32 USEProgression::TechLevel(const FString& Prefix) const
{
    int32 N = 0;
    for (FName T : Data.UnlockedTech) { if (T.ToString().StartsWith(Prefix)) ++N; }
    return N;
}

void USEProgression::AddRep(FName Faction, int32 Delta)
{
    if (Faction.IsNone()) return;
    int32& R = Data.Reputation.FindOrAdd(Faction);
    R = FMath::Clamp(R + Delta, -100, 100);
}

bool USEProgression::AddDiscovery(FName Id, const FString& Name, const FString& Info, int32 Research)
{
    if (Data.Discoveries.Contains(Id)) return false;
    Data.Discoveries.Add(Id, Name + TEXT(" - ") + Info);
    AddResearch(Research);
    AddXP(Research);
    return true;
}

// ---------------------------------------------------------------- missions
bool USEProgression::IsMissionTaken(FName Id) const
{
    if (Data.CompletedMissions.Contains(Id)) return true;
    for (const FSEMission& M : Data.ActiveMissions) if (M.Id == Id) return true;
    return false;
}

bool USEProgression::AcceptMission(const FSEMission& M)
{
    if (IsMissionTaken(M.Id) || Data.ActiveMissions.Num() >= 6) return false;
    Data.ActiveMissions.Add(M);
    PushNotice(TEXT("Mission accepted: ") + M.Title);
    return true;
}

void USEProgression::AbandonMission(int32 Index)
{
    if (Data.ActiveMissions.IsValidIndex(Index)) Data.ActiveMissions.RemoveAt(Index);
}

void USEProgression::ProgressMissions(int32 Type, FName Target, int32 Amount)
{
    for (FSEMission& M : Data.ActiveMissions)
    {
        if (M.Type == Type && (M.Target.IsNone() || M.Target == Target) && !M.IsComplete())
            M.Progress = FMath::Min(M.Required, M.Progress + Amount);
    }
    CheckCompletions();
}

void USEProgression::CheckLocation(const FVector& Loc, int32 System, int32 Planet)
{
    bool bAny = false;
    for (FSEMission& M : Data.ActiveMissions)
    {
        if (M.Type == 3 && !M.IsComplete() && M.SystemIdx == System && M.PlanetIdx == Planet && FVector::Dist2D(Loc, M.Location) < 3000.f)
        {
            M.Progress = M.Required; bAny = true;
        }
    }
    if (bAny) CheckCompletions();
}

void USEProgression::CheckCompletions()
{
    for (int32 i = Data.ActiveMissions.Num() - 1; i >= 0; --i)
    {
        const FSEMission M = Data.ActiveMissions[i];
        if (!M.IsComplete()) continue;
        Data.ActiveMissions.RemoveAt(i);
        Data.CompletedMissions.Add(M.Id);
        AddCredits(M.RewardCredits);
        AddResearch(M.RewardResearch);
        AddXP(M.RewardResearch + 10);
        AddRep(M.Faction, M.RewardRep);
        PushNotice(FString::Printf(TEXT("Mission complete: %s  (+%d cr, +%d research, +%d rep)"), *M.Title, M.RewardCredits, M.RewardResearch, M.RewardRep));
    }
}

TArray<FSEMission> USEProgression::GenerateMissions(int32 Seed, FName Faction, const FString& Giver, int32 Sys, int32 Planet, const FVector& Origin, const TArray<FName>& SpeciesIds)
{
    static const TCHAR* Ores[] = { TEXT("iron_ore"), TEXT("copper_ore"), TEXT("carbon"), TEXT("silicon"), TEXT("ice") };
    TArray<FSEMission> Out;
    FRandomStream R(Seed);
    for (int32 i = 0; i < 3; ++i)
    {
        FSEMission M;
        M.Id = FName(*FString::Printf(TEXT("m%d_%d"), Seed, i));
        M.Faction = Faction; M.GiverName = Giver; M.SystemIdx = Sys; M.PlanetIdx = Planet;
        int32 Type = R.RandRange(0, 3);
        if (Type == 2 && SpeciesIds.Num() == 0) Type = 0;
        M.Type = Type;
        switch (Type)
        {
        case 0:
        {
            const FName Ore(Ores[R.RandRange(0, 4)]);
            M.Target = Ore; M.Required = R.RandRange(8, 20);
            M.Title = FString::Printf(TEXT("Gather %d x %s"), M.Required, *Ore.ToString().Replace(TEXT("_"), TEXT(" ")));
            M.Desc = TEXT("Mine the resource with your multitool (left mouse) and the work counts automatically.");
            M.RewardCredits = M.Required * 14;
            break;
        }
        case 1:
            M.Required = R.RandRange(3, 6);
            M.Title = FString::Printf(TEXT("Survey: scan %d things"), M.Required);
            M.Desc = TEXT("Point at ore, creatures or structures and press T. Each new or repeat scan counts.");
            M.RewardCredits = M.Required * 30; M.RewardResearch = M.Required * 4;
            break;
        case 2:
        {
            M.Target = SpeciesIds[R.RandRange(0, SpeciesIds.Num() - 1)];
            M.Required = R.RandRange(2, 4);
            M.Title = FString::Printf(TEXT("Cull %d dangerous creatures"), M.Required);
            M.Desc = TEXT("Hunt the marked species with your multitool. Open the encyclopedia after scanning one to learn its name.");
            M.RewardCredits = M.Required * 55;
            break;
        }
        default:
        {
            const float Ang = R.FRandRange(0.f, TWO_PI), Dist = R.FRandRange(40000.f, 120000.f);
            M.Location = Origin + FVector(FMath::Cos(Ang) * Dist, FMath::Sin(Ang) * Dist, 0.f);
            M.Required = 1;
            M.Title = FString::Printf(TEXT("Reach the beacon %.0f m away"), Dist / 100.f);
            M.Desc = TEXT("Travel on foot or by ship to the waypoint shown on your HUD.");
            M.RewardCredits = 150; M.RewardResearch = 8;
            break;
        }
        }
        M.RewardRep = R.RandRange(5, 12);
        M.RewardResearch += R.RandRange(2, 6);
        Out.Add(M);
    }
    return Out;
}

// ---------------------------------------------------------------- lifecycle / settings
void USEProgression::ResetForNewGame()
{
    Data = FSEProgressSave();
    Notices.Reset();
}

void USEProgression::SaveSettings() const
{
    USESettingsSave* S = Cast<USESettingsSave>(UGameplayStatics::CreateSaveGameObject(USESettingsSave::StaticClass()));
    S->Settings = Settings;
    UGameplayStatics::SaveGameToSlot(S, TEXT("settings"), 0);
}

void USEProgression::LoadSettings()
{
    if (!UGameplayStatics::DoesSaveGameExist(TEXT("settings"), 0)) return;
    if (USESettingsSave* S = Cast<USESettingsSave>(UGameplayStatics::LoadGameFromSlot(TEXT("settings"), 0))) { Settings = S->Settings; }
}
