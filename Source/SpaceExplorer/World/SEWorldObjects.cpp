#include "SEWorldObjects.h"
#include "SEPlayerCharacter.h"
#include "SEInventoryComponent.h"
#include "SEVisual.h"
#include "SEGameData.h"
#include "SEGalaxyTypes.h"
#include "SENPC.h"
#include "SETerrainStreamer.h"
#include "SEGalaxySubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

static USEProgression* Prog(const UObject* O) { return O->GetWorld()->GetGameInstance()->GetSubsystem<USEProgression>(); }

// ---------------------------------------------------------------- resource node
ASEResourceNode::ASEResourceNode()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    RootComponent = Mesh;
}
void ASEResourceNode::InitNode(FName Item, int32 Amount, int32 Key, const FLinearColor& Color, float Scale)
{
    ItemId = Item; Remaining = MaxAmount = FMath::Max(1, Amount); NodeKey = Key; BaseScale = Scale;
    SEVisual::Setup(Mesh, TEXT("Sphere"), Color, FVector(Scale));
}
FString ASEResourceNode::GetScanName() const
{
    return Prog(this)->GetGameInstance()->GetSubsystem<USEGameData>()->ItemName(ItemId) + TEXT(" deposit");
}
int32 ASEResourceNode::Mine(ASEPlayerCharacter* P, int32 Amount)
{
    if (!P || Amount <= 0 || Remaining <= 0) return 0;
    USEProgression* Pr = Prog(this);
    const int32 Added = P->Inventory->AddItem(ItemId, FMath::Min(Amount, Remaining));
    if (Added <= 0) { Pr->PushNotice(TEXT("Inventory full")); return 0; }
    Remaining -= Added;
    Pr->ProgressMissions(0, ItemId, Added);
    Pr->AddXP(1);
    if (Remaining <= 0) { Pr->MarkDepleted(NodeKey); Destroy(); }
    else { Mesh->SetRelativeScale3D(FVector(BaseScale * FMath::Lerp(0.5f, 1.f, (float)Remaining / MaxAmount))); }
    return Added;
}

// ---------------------------------------------------------------- ruin
ASERuin::ASERuin() { RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); }
void ASERuin::InitRuin(int32 Key)
{
    RuinKey = Key;
    const FLinearColor Stone(0.28f, 0.32f, 0.36f);
    for (int32 i = 0; i < 6; ++i)
    {
        const float A = i * TWO_PI / 6.f;
        SEVisual::AddMesh(this, RootComponent, TEXT("Cylinder"), Stone, FVector(FMath::Cos(A) * 500.f, FMath::Sin(A) * 500.f, 250.f), FVector(1.f, 1.f, 5.f));
    }
    SEVisual::AddMesh(this, RootComponent, TEXT("Cube"), Stone * 0.8f, FVector(0.f, 0.f, 300.f), FVector(1.5f, 0.6f, 6.f));
    SEVisual::AddMesh(this, RootComponent, TEXT("Sphere"), FLinearColor(0.2f, 0.9f, 0.9f), FVector(0.f, 0.f, 680.f), FVector(0.8f), false);
}
FText ASERuin::GetInteractPrompt() const
{
    return Prog(this)->IsDepleted(RuinKey) ? FText::FromString(TEXT("Ruin (already examined)")) : FText::FromString(TEXT("Press F to examine the ruin"));
}
void ASERuin::Interact(ASEPlayerCharacter* P)
{
    USEProgression* Pr = Prog(this);
    if (Pr->IsDepleted(RuinKey)) { Pr->PushNotice(TEXT("You have already examined these ruins.")); return; }
    const int32 Got = P->Inventory->AddItem(TEXT("alien_artifact"), 1);
    if (Got <= 0) { Pr->PushNotice(TEXT("Inventory full")); return; }
    Pr->MarkDepleted(RuinKey);
    Pr->AddResearch(25); Pr->AddXP(25);
    Pr->PushNotice(TEXT("Found an Alien Artifact! (+25 research)"));
}

// ---------------------------------------------------------------- base structure
ASEBaseStructure::ASEBaseStructure()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetMobility(EComponentMobility::Movable);
    RootComponent = Mesh;
}
void ASEBaseStructure::InitStructure(FName InType, bool bGhost)
{
    Type = InType; bIsGhost = bGhost;
    const FSEStructureDef* D = GetWorld()->GetGameInstance()->GetSubsystem<USEGameData>()->FindStructure(Type);
    if (!D) return;
    SEVisual::Setup(Mesh, TEXT("Cube"), D->Color, D->Size);
    Mesh->SetCollisionProfileName(bGhost ? TEXT("NoCollision") : TEXT("BlockAll"));
}
void ASEBaseStructure::SetGhostColor(bool bAffordable)
{
    SEVisual::Colorize(Mesh, bAffordable ? FLinearColor(0.2f, 0.9f, 0.3f) : FLinearColor(0.9f, 0.2f, 0.2f));
}
FText ASEBaseStructure::GetInteractPrompt() const
{
    const FSEStructureDef* D = GetWorld()->GetGameInstance()->GetSubsystem<USEGameData>()->FindStructure(Type);
    return FText::FromString(FString::Printf(TEXT("Press F to dismantle %s (50%% refund)"), D ? *D->Name : TEXT("structure")));
}
void ASEBaseStructure::Interact(ASEPlayerCharacter* P)
{
    USEGameData* GD = GetWorld()->GetGameInstance()->GetSubsystem<USEGameData>();
    USEProgression* Pr = Prog(this);
    if (const FSEStructureDef* D = GD->FindStructure(Type))
        for (const TPair<FName, int32>& C : D->Cost) { P->Inventory->AddItem(C.Key, C.Value / 2); }
    for (int32 i = Pr->Data.Built.Num() - 1; i >= 0; --i)
        if (Pr->Data.Built[i].Type == Type && FVector::DistSquared(Pr->Data.Built[i].Location, GetActorLocation()) < 100.f) { Pr->Data.Built.RemoveAt(i); break; }
    Destroy();
}
ASEBaseStructure::FZone ASEBaseStructure::QueryZone(UWorld* World, const FVector& Loc)
{
    FZone Z;
    for (TActorIterator<ASEBaseStructure> It(World); It; ++It)
    {
        if (It->bIsGhost) continue;
        const float D = FVector::Dist(It->GetActorLocation(), Loc);
        const FString T = It->Type.ToString();
        if (T == TEXT("habitat") && D < 800.f)        { Z.bShelter = true; Z.bHeal = true; }
        if (T == TEXT("oxygen_beacon") && D < 3000.f) { Z.bShelter = true; }
        if (T == TEXT("solar") && D < 1500.f)         { Z.bSolar = true; }
        if (T == TEXT("fabricator") && D < 1500.f)    { Z.bFabricator = true; }
    }
    return Z;
}

// ---------------------------------------------------------------- debris
ASEDebris::ASEDebris()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetMobility(EComponentMobility::Movable);
    RootComponent = Mesh;
    InitialLifeSpan = 10.f;
}
void ASEDebris::Launch(const FLinearColor& Color, const FVector& Impulse)
{
    SEVisual::Setup(Mesh, TEXT("Cube"), Color, FVector(FMath::FRandRange(0.3f, 1.2f)));
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(true);
    Mesh->AddImpulse(Impulse, NAME_None, true);
}

// ---------------------------------------------------------------- settlement
ASESettlement::ASESettlement() { RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); }

void ASESettlement::InitSettlement(int32 Seed, FName Faction, const FVector& Center, ASETerrainStreamer* T, int32 PlanetSeed, const TArray<FName>& SpeciesIds, int32 Sys, int32 Planet)
{
    FRandomStream R(Seed);
    USEGameData* GD = GetWorld()->GetGameInstance()->GetSubsystem<USEGameData>();
    const FSEFaction* F = GD->FindFaction(Faction);
    FactionName = F ? F->Name : TEXT("Unknown");
    const FLinearColor FC = F ? F->Color : FLinearColor::Gray;
    static const TCHAR* Syl[] = { TEXT("Ar"), TEXT("Vel"), TEXT("Kor"), TEXT("Nim"), TEXT("Tas"), TEXT("Ur"), TEXT("Del"), TEXT("Ish") };
    SettlementName = FString(Syl[R.RandRange(0, 7)]) + Syl[R.RandRange(0, 7)] + TEXT(" Outpost");

    SetActorLocation(Center);
    auto Rel = [&](float X, float Y, float ZOff) { return FVector(X - Center.X, Y - Center.Y, T->GetHeightAt(X, Y) + ZOff - Center.Z); };
    SEVisual::AddMesh(this, RootComponent, TEXT("Cylinder"), FLinearColor(0.22f, 0.22f, 0.25f), Rel(Center.X, Center.Y, 10.f), FVector(18.f, 18.f, 0.2f));
    SEVisual::AddMesh(this, RootComponent, TEXT("Cylinder"), FC, Rel(Center.X, Center.Y, 400.f), FVector(0.5f, 0.5f, 8.f));

    TArray<FVector> Fronts;
    for (int32 i = 0; i < 4; ++i)
    {
        const float A = i * HALF_PI + R.FRandRange(-0.25f, 0.25f);
        const float BX = Center.X + FMath::Cos(A) * 1500.f, BY = Center.Y + FMath::Sin(A) * 1500.f;
        SEVisual::AddMesh(this, RootComponent, TEXT("Cube"), FC * 0.55f, Rel(BX, BY, 150.f), FVector(7.f, 7.f, 4.f));
        const float FX = Center.X + FMath::Cos(A) * 1000.f, FY = Center.Y + FMath::Sin(A) * 1000.f;
        Fronts.Add(FVector(FX, FY, T->GetHeightAt(FX, FY) + 100.f));
    }

    TArray<FString> Jobs;
    if (Faction == TEXT("wayfarers")) Jobs = { TEXT("Trader"), TEXT("Trader"), TEXT("Engineer"), TEXT("Scientist") };
    else if (Faction == TEXT("verdant")) Jobs = { TEXT("Trader"), TEXT("Scientist"), TEXT("Scientist"), TEXT("Engineer") };
    else Jobs = { TEXT("Trader"), TEXT("Miner"), TEXT("Engineer"), TEXT("Miner") };

    const int32 Count = R.RandRange(3, 4);
    for (int32 i = 0; i < Count; ++i)
    {
        const float A = R.FRandRange(0.f, TWO_PI), D = R.FRandRange(300.f, 700.f);
        const FVector Social(Center.X + FMath::Cos(A) * D, Center.Y + FMath::Sin(A) * D, T->GetHeightAt(Center.X + FMath::Cos(A) * D, Center.Y + FMath::Sin(A) * D) + 100.f);
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ASENPC* N = GetWorld()->SpawnActor<ASENPC>(ASENPC::StaticClass(), Social + FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator, P))
        {
            N->InitNPC(HashCombine(Seed, i + 1), Jobs[i % Jobs.Num()], Faction, Fronts[i % 4], Fronts[(i + 1) % 4], Social, PlanetSeed, SpeciesIds, Sys, Planet);
            Residents.Add(N);
        }
    }
}

void ASESettlement::EndPlay(const EEndPlayReason::Type Reason)
{
    for (TWeakObjectPtr<ASENPC>& N : Residents) { if (N.IsValid()) N->Destroy(); }
    Super::EndPlay(Reason);
}

void ASEBaseStructure::SpawnAllBuilt(UWorld* W)
{
    TArray<ASEBaseStructure*> Old;
    for (TActorIterator<ASEBaseStructure> It(W); It; ++It) Old.Add(*It);
    for (ASEBaseStructure* S : Old) S->Destroy();
    USEProgression* Pr = W->GetGameInstance()->GetSubsystem<USEProgression>();
    USEGalaxySubsystem* G = W->GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    if (!Pr || !G) return;
    for (const FSEBuiltStructure& B : Pr->Data.Built)
    {
        if (B.SystemIdx != G->CurrentSystem || B.PlanetIdx != G->CurrentPlanet) continue;
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ASEBaseStructure* S = W->SpawnActor<ASEBaseStructure>(ASEBaseStructure::StaticClass(), B.Location, FRotator(0.f, B.Yaw, 0.f), P)) { S->InitStructure(B.Type, false); }
    }
}
