#include "SENPC.h"
#include "SEPlayerCharacter.h"
#include "SEPlayerController.h"
#include "SEVisual.h"
#include "SEGameData.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"

void ASENPC::InitNPC(int32 Seed, const FString& InJob, FName InFaction, const FVector& InHome, const FVector& InWork, const FVector& InSocial,
                     int32 InPlanetSeed, const TArray<FName>& SpeciesIds, int32 Sys, int32 Planet)
{
    FRandomStream R(Seed);
    static const TCHAR* First[] = { TEXT("Ka"), TEXT("Ven"), TEXT("Ori"), TEXT("Thal"), TEXT("Zeph"), TEXT("Mira"), TEXT("Dax"), TEXT("Lun"), TEXT("Sael"), TEXT("Brin") };
    static const TCHAR* Last[] = { TEXT("-tor"), TEXT("-ixa"), TEXT("-mere"), TEXT("-os"), TEXT("-ael"), TEXT("-quen"), TEXT("-vash"), TEXT("-ine") };
    NPCName = FString(First[R.RandRange(0, UE_ARRAY_COUNT(First) - 1)]) + Last[R.RandRange(0, UE_ARRAY_COUNT(Last) - 1)];
    Job = InJob; Faction = InFaction; PlanetSeed = InPlanetSeed;
    HomePos = InHome; WorkPos = InWork; SocialPos = InSocial;
    Progression = GetGameInstance()->GetSubsystem<USEProgression>();
    WalkSpeed = 220.f;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

    if (Job == TEXT("Trader"))         { Stock = { TEXT("fuel_cell"), TEXT("oxygen_canister"), TEXT("repair_kit"), TEXT("iron_plate"), TEXT("circuit") };
        Lines = { TEXT("Prices shift from planet to planet. Buy low, sell high."), TEXT("The Guild keeps the lanes open. A full tank is cheap insurance."), TEXT("Got surplus fuel cells if you are heading out.") }; }
    else if (Job == TEXT("Miner"))     { Stock = { TEXT("iron_ore"), TEXT("copper_ore"), TEXT("carbon"), TEXT("ice"), TEXT("silicon"), TEXT("uranium") };
        Lines = { TEXT("Deposits glitter on the horizon if you know where to look."), TEXT("Point your multitool and hold the button. Nodes shrink as you mine."), TEXT("Uranium is rare, but it pays.") }; }
    else if (Job == TEXT("Scientist")) { Stock = { TEXT("circuit"), TEXT("oxygen_canister"), TEXT("silicon") };
        Lines = { TEXT("Every creature here has a story. Scan them: research pays for upgrades."), TEXT("Ruins like the ones out there hold artifacts the Collective will buy."), TEXT("Press T to scan. Open the encyclopedia to review.") }; }
    else                               { Stock = { TEXT("iron_plate"), TEXT("circuit"), TEXT("repair_kit"), TEXT("fuel_cell") };
        Lines = { TEXT("A ship is only as good as its fuel and plating."), TEXT("Build a Fabricator and you can craft circuits and repair kits yourself."), TEXT("Habitats keep the air breathable. Beacons cover more ground.") }; }

    const FSEFaction* F = GetGameInstance()->GetSubsystem<USEGameData>()->FindFaction(Faction);
    const FLinearColor C = F ? F->Color : FLinearColor::Gray;
    GetCapsuleComponent()->SetCapsuleSize(35.f, 90.f);
    SEVisual::Setup(Body, TEXT("Cylinder"), C, FVector(0.6f, 0.6f, 1.5f));
    SEVisual::AddMesh(this, Body, TEXT("Sphere"), C * 1.3f, FVector(0.f, 0.f, 100.f), FVector(0.45f, 0.45f, 0.45f), false);

    Offers = USEProgression::GenerateMissions(Seed, Faction, NPCName, Sys, Planet, SocialPos, SpeciesIds);
}

FString ASENPC::GetActivity() const
{
    const float H = Progression ? Progression->Data.WorldHours : 12.f;
    if (H >= 8.f && H < 17.f) return TEXT("Working");
    if (H >= 17.f && H < 21.f) return TEXT("Socialising");
    return TEXT("Resting");
}

FString ASENPC::GetGreeting() const
{
    const int32 Rep = Progression ? Progression->GetRep(Faction) : 0;
    if (Rep < -30) return TEXT("Keep your distance. We have not forgotten what you did.");
    if (Rep > 30) return TEXT("Good to see you again, friend!");
    return TEXT("Hello, traveler.");
}

FText ASENPC::GetInteractPrompt() const
{
    return FText::FromString(FString::Printf(TEXT("Press F to talk to %s (%s)"), *NPCName, *Job));
}

void ASENPC::Interact(ASEPlayerCharacter* Interactor)
{
    if (!Interactor) return;
    if (ASEPlayerController* PC = Cast<ASEPlayerController>(Interactor->GetController())) { PC->OpenDialogue(this); }
}

void ASENPC::TakeToolDamage(float Amount, AActor* Source)
{
    Health -= Amount;
    if (Progression)
    {
        Progression->AddRep(Faction, -10);
        Progression->PushNotice(FString::Printf(TEXT("%s does not appreciate that (-10 reputation)"), *NPCName));
    }
    if (Health <= 0.f)
    {
        if (Progression) { Progression->AddRep(Faction, -40); Progression->PushNotice(TEXT("You killed a civilian. The faction will remember (-40)")); }
        Destroy();
    }
}

void ASENPC::Think(float Dt)
{
    const FString A = GetActivity();
    const FVector Goal = (A == TEXT("Working")) ? WorkPos : (A == TEXT("Socialising") ? SocialPos : HomePos);
    MoveToward(Goal, 0.6f, 200.f);
}
