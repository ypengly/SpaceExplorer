#include "SECreature.h"
#include "SEPlayerCharacter.h"
#include "SEVitalsComponent.h"
#include "SEInventoryComponent.h"
#include "SEProgression.h"
#include "SEVisual.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

// ------------------------------------------------------------ species generation
TArray<FSESpecies> SECreatureGen::Generate(const FSEPlanetData& P)
{
    static const TCHAR* Syl[] = { TEXT("ka"),TEXT("vel"),TEXT("or"),TEXT("thi"),TEXT("mus"),TEXT("zen"),TEXT("dra"),TEXT("syl"),TEXT("quo"),TEXT("rin"),TEXT("tal"),TEXT("eph"),TEXT("bu"),TEXT("nex") };
    TArray<FSESpecies> Out;
    FRandomStream R(GetTypeHash(FIntPoint(P.Seed, 777)));
    const bool bHarsh = (P.Biome == ESEBiome::Barren) || !P.bHasAtmosphere;

    struct FArch { const TCHAR* Suffix; int32 Diet; float Size, Speed, Health, Damage; bool Pack, Night, Terr, Skit; };
    static const FArch Arch[] = {
        { TEXT("grazer"),   0, 1.0f, 280.f,  35.f,  0.f, true,  false, false, true  },
        { TEXT("lurker"),   0, 2.2f, 200.f, 140.f, 18.f, false, false, true,  false },
        { TEXT("stalker"),  1, 1.1f, 520.f,  60.f, 12.f, false, false, false, false },
        { TEXT("runner"),   1, 0.8f, 480.f,  35.f,  7.f, true,  true,  false, false },
    };
    for (int32 i = 0; i < 4; ++i)
    {
        if (bHarsh && (i == 1 || i == 3)) continue;
        const FArch& A = Arch[i];
        FSESpecies S;
        S.Id = FName(*FString::Printf(TEXT("sp_%d_%d"), P.Seed, i));
        FString Name; const int32 N = R.RandRange(2, 3);
        for (int32 k = 0; k < N; ++k) Name += Syl[R.RandRange(0, UE_ARRAY_COUNT(Syl) - 1)];
        Name[0] = FChar::ToUpper(Name[0]);
        S.Name = Name + TEXT(" ") + A.Suffix;
        S.Diet = A.Diet; S.Size = A.Size * R.FRandRange(0.85f, 1.2f); S.Speed = A.Speed; S.Health = A.Health; S.Damage = A.Damage;
        S.bPack = A.Pack; S.bNocturnal = A.Night; S.bTerritorial = A.Terr; S.bSkittish = A.Skit;
        float Hue = R.FRandRange(0.f, 255.f);
        switch (P.Biome) { case ESEBiome::Volcanic: Hue = R.FRandRange(0.f, 25.f); break; case ESEBiome::Frozen: Hue = R.FRandRange(120.f, 160.f); break; case ESEBiome::Desert: Hue = R.FRandRange(15.f, 40.f); break; default: break; }
        S.Color = FLinearColor::MakeFromHSV8((uint8)Hue, (uint8)(bHarsh ? 40 : R.RandRange(110, 220)), (uint8)(A.Diet == 1 ? 150 : 210));
        S.ScanReward = A.Diet == 1 ? 25 : 15;
        S.Desc = FString::Printf(TEXT("%s. %s. %s%s%s Roughly %.1f m long."), A.Diet == 0 ? TEXT("Herbivore") : TEXT("Carnivore"),
            A.Skit ? TEXT("Skittish, flees on sight") : (A.Terr ? TEXT("Fiercely territorial") : TEXT("Aggressive when hungry")),
            A.Pack ? TEXT("Lives in groups. ") : TEXT("Solitary. "), A.Night ? TEXT("Active at night. ") : TEXT("Active by day. "), TEXT(""), S.Size * 1.2f);
        Out.Add(S);
    }
    return Out;
}

// ------------------------------------------------------------ creature
void ASECreature::InitCreature(const FSESpecies& S, ASECreature* InLeader, const FVector& InHome, bool bJuvenile)
{
    Species = S;
    Leader = InLeader;
    Home = InHome;
    Health = S.Health;
    Hunger = FMath::FRandRange(0.f, 0.5f);
    ReproTimer = FMath::FRandRange(90.f, 240.f);
    Age = bJuvenile ? 0.f : 999.f;
    Progression = GetGameInstance()->GetSubsystem<USEProgression>();

    GetCapsuleComponent()->SetCapsuleSize(40.f * S.Size, 45.f * S.Size);
    WalkSpeed = S.Speed;
    GetCharacterMovement()->MaxWalkSpeed = S.Speed;
    SEVisual::Setup(Body, TEXT("Sphere"), S.Color, FVector(S.Size * 1.1f, S.Size * 0.65f, S.Size * 0.65f));
    SEVisual::AddMesh(this, Body, TEXT("Sphere"), S.Color * 0.8f, FVector(S.Size * 55.f, 0.f, S.Size * 18.f), FVector(0.45f, 0.45f, 0.45f), false);
    Body->SetRelativeLocation(FVector::ZeroVector);
    MoveTarget = GetActorLocation();
}

int32 ASECreature::CountSpecies(UWorld* World, FName SpeciesId)
{
    int32 N = 0;
    for (TActorIterator<ASECreature> It(World); It; ++It) { if (!It->bDead && It->Species.Id == SpeciesId) ++N; }
    return N;
}

FVector ASECreature::RandomPointAround(const FVector& C, float Radius) const
{
    const float A = FMath::FRandRange(0.f, TWO_PI), D = FMath::FRandRange(Radius * 0.3f, Radius);
    return C + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
}

void ASECreature::TakeToolDamage(float Amount, AActor* Source)
{
    if (bDead) return;
    Health -= Amount;
    if (Health <= 0.f) { Die(Source); return; }
    if (Species.bSkittish && !Species.bTerritorial) { Threat = Source; SetState(EState::Flee, 5.f); }
    else { Target = Source; SetState(Species.bTerritorial ? EState::Defend : EState::Hunt, 12.f); }
}

void ASECreature::Die(AActor* Killer)
{
    bDead = true;
    if (ASEPlayerCharacter* P = Cast<ASEPlayerCharacter>(Killer))
    {
        const int32 N = FMath::Max(1, FMath::RoundToInt(Species.Size * 3.f));
        const int32 Got = P->Inventory->AddItem(TEXT("biomass"), N);
        if (Progression)
        {
            Progression->ProgressMissions(0, TEXT("biomass"), Got);
            Progression->ProgressMissions(2, Species.Id, 1);
            Progression->Data.Kills.FindOrAdd(Species.Id)++;
            Progression->AddXP(5 + FMath::RoundToInt(Species.Size * 5.f));
            Progression->PushNotice(FString::Printf(TEXT("Defeated %s (+%d biomass)"), *Species.Name, Got));
        }
    }
    SetLifeSpan(0.1f);
}

void ASECreature::Sense()
{
    SenseTimer = 0.5f;
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    ASEPlayerCharacter* Player = Cast<ASEPlayerCharacter>(PlayerPawn);   // null while the player is inside the ship
    const FVector Loc = GetActorLocation();
    const float DPlayer = Player ? FVector::Dist(Player->GetActorLocation(), Loc) : 1.0e9f;

    if (Species.bSkittish && State != EState::Flee && DPlayer < 1400.f) { Threat = Player; SetState(EState::Flee, 4.f); return; }

    if (Species.bTerritorial && Player)
    {
        if (State != EState::Defend && DPlayer < 1200.f && FVector::Dist2D(Loc, Home) < 2500.f) { Target = Player; SetState(EState::Defend, 15.f); return; }
        if (State == EState::Defend && (DPlayer > 3000.f || FVector::Dist2D(Loc, Home) > 4500.f)) { SetState(EState::Idle, 2.f); }
    }

    if (Species.Diet == 1 && State != EState::Hunt && State != EState::Flee)
    {
        if (Player && DPlayer < 1600.f && Hunger > 0.3f) { Target = Player; SetState(EState::Hunt, 15.f); return; }
        if (Hunger > 0.5f)
        {
            ASECreature* Best = nullptr; float BestD = 2500.f;
            for (TActorIterator<ASECreature> It(GetWorld()); It; ++It)
            {
                if (*It == this || It->bDead || It->Species.Diet != 0) continue;
                const float D = FVector::Dist(It->GetActorLocation(), Loc);
                if (D < BestD) { BestD = D; Best = *It; }
            }
            if (Best) { Target = Best; SetState(EState::Hunt, 15.f); }
        }
    }
}

void ASECreature::Think(float Dt)
{
    if (bDead) return;
    Hunger = FMath::Min(1.f, Hunger + Dt / 200.f);
    ReproTimer -= Dt; StateTimer -= Dt; AttackCooldown -= Dt; SenseTimer -= Dt;
    Health = FMath::Min(Species.Health, Health + Dt * 0.5f);

    if (Age < 120.f)
    {
        Age += Dt;
        const float G = FMath::Lerp(0.55f, 1.f, Age / 120.f);
        Body->SetRelativeScale3D(FVector(Species.Size * 1.1f, Species.Size * 0.65f, Species.Size * 0.65f) * G);
    }

    const float Hour = Progression ? Progression->Data.WorldHours : 12.f;
    const bool bNight = (Hour < 6.f || Hour >= 20.f);
    const bool bSleepTime = Species.bNocturnal ? !bNight : bNight;
    if (bSleepTime && State != EState::Flee && State != EState::Defend && State != EState::Hunt) { State = EState::Sleep; return; }
    if (!bSleepTime && State == EState::Sleep) { SetState(EState::Idle, 1.f); }

    if (SenseTimer <= 0.f) Sense();

    const FVector Loc = GetActorLocation();
    AActor* Tgt = Target.Get();

    switch (State)
    {
    case EState::Idle:
        if (StateTimer <= 0.f)
        {
            if (Hunger > 0.65f && Species.Diet == 0) { MoveTarget = RandomPointAround(Home, 2500.f); SetState(EState::Feed, 30.f); }
            else
            {
                const FVector Center = (Leader.IsValid() && Leader.Get() != this) ? Leader->GetActorLocation() : Home;
                MoveTarget = RandomPointAround(Center, (Leader.IsValid() && Leader.Get() != this) ? 500.f : 1500.f);
                SetState(EState::Wander, 15.f);
            }
        }
        break;
    case EState::Wander:
        if (MoveToward(MoveTarget, 0.5f, 150.f) || StateTimer <= 0.f) { SetState(EState::Idle, FMath::FRandRange(3.f, 8.f)); }
        break;
    case EState::Feed:
        if (!bArrived) { if (MoveToward(MoveTarget, 0.6f, 150.f)) { bArrived = true; StateTimer = 5.f; } else if (StateTimer <= 0.f) SetState(EState::Idle, 2.f); }
        else if (StateTimer <= 0.f) { Hunger = 0.f; SetState(EState::Idle, 3.f); }
        break;
    case EState::Flee:
    {
        AActor* T = Threat.Get();
        const FVector Away = T ? (Loc - T->GetActorLocation()).GetSafeNormal2D() : FVector::ForwardVector;
        MoveToward(Loc + Away * 1500.f, 1.5f, 50.f);
        if (StateTimer <= 0.f) SetState(EState::Idle, 2.f);
        break;
    }
    case EState::Hunt:
    case EState::Defend:
    {
        ASECreature* PreyCreature = Cast<ASECreature>(Tgt);
        if (!Tgt || (PreyCreature && PreyCreature->bDead) || StateTimer <= 0.f || FVector::Dist(Tgt->GetActorLocation(), Loc) > 3500.f)
        {
            if (PreyCreature && PreyCreature->bDead) Hunger = 0.f;
            SetState(EState::Idle, 3.f);
            break;
        }
        const float Reach = 230.f * Species.Size;
        if (MoveToward(Tgt->GetActorLocation(), 1.3f, Reach * 0.8f) || FVector::Dist(Tgt->GetActorLocation(), Loc) < Reach)
        {
            if (AttackCooldown <= 0.f)
            {
                AttackCooldown = 1.2f;
                if (ASEPlayerCharacter* P = Cast<ASEPlayerCharacter>(Tgt)) { P->Vitals->ApplyDamage(Species.Damage); }
                else if (PreyCreature) { PreyCreature->TakeToolDamage(Species.Damage * 2.f, this); }
            }
        }
        break;
    }
    default: break;
    }

    // Reproduction: well-fed adults with a nearby mate, capped per species.
    if (ReproTimer <= 0.f && Hunger < 0.35f && Age >= 120.f && State != EState::Flee)
    {
        ReproTimer = 120.f;
        if (CountSpecies(GetWorld(), Species.Id) < 10)
        {
            for (TActorIterator<ASECreature> It(GetWorld()); It; ++It)
            {
                if (*It == this || It->bDead || It->Species.Id != Species.Id || It->Age < 120.f) continue;
                if (FVector::Dist(It->GetActorLocation(), Loc) > 1200.f) continue;
                FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
                if (ASECreature* Child = GetWorld()->SpawnActor<ASECreature>(GetClass(), Loc + FVector(150.f, 0.f, 80.f), FRotator::ZeroRotator, P))
                {
                    Child->InitCreature(Species, Leader.IsValid() ? Leader.Get() : this, Home, true);
                    ResetReproduction(); It->ResetReproduction();
                }
                break;
            }
        }
    }
}
