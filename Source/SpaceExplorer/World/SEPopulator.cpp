#include "SEPopulator.h"
#include "Misc/Optional.h"
#include "SETerrainStreamer.h"
#include "SEWorldObjects.h"
#include "SEVisual.h"
#include "SEProgression.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

ASEChunkDecor::ASEChunkDecor() { RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); }

void ASEChunkDecor::BuildDecor(FIntPoint Coord, const FSEPlanetData& P, ASETerrainStreamer* T)
{
    FRandomStream R(HashCombine(P.Seed, HashCombine(Coord.X * 83492791, Coord.Y * 297121507)));
    const float CS = T->ChunkSize, Sea = T->GetSeaLevel();

    auto Make = [this](const TCHAR* Mesh, const FLinearColor& Col, bool bCollide)
    {
        UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
        C->SetMobility(EComponentMobility::Movable);
        C->SetupAttachment(RootComponent);
        C->RegisterComponent();
        C->SetStaticMesh(SEVisual::Basic(Mesh));
        SEVisual::Colorize(C, Col);
        C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
        C->SetCastShadow(false);
        return C;
    };
    auto Place = [&](float MinSea) -> TOptional<FVector>
    {
        const float X = (Coord.X + R.FRand()) * CS, Y = (Coord.Y + R.FRand()) * CS;
        const float H = T->GetHeightAt(X, Y);
        if (H < Sea + MinSea) return {};
        return FVector(X, Y, H);
    };

    UInstancedStaticMeshComponent* A = nullptr; UInstancedStaticMeshComponent* B = nullptr; UInstancedStaticMeshComponent* Rocks = Make(TEXT("Sphere"), FLinearColor(0.35f, 0.33f, 0.31f), true);
    int32 Count = 0;
    switch (P.Biome)
    {
    case ESEBiome::Temperate: A = Make(TEXT("Cylinder"), FLinearColor(0.3f, 0.2f, 0.1f), true); B = Make(TEXT("Sphere"), FLinearColor(0.12f, 0.5f, 0.15f), false); Count = 28; break;
    case ESEBiome::Ocean:     A = Make(TEXT("Cylinder"), FLinearColor(0.35f, 0.25f, 0.12f), true); B = Make(TEXT("Sphere"), FLinearColor(0.2f, 0.6f, 0.3f), false); Count = 12; break;
    case ESEBiome::Desert:    A = Make(TEXT("Cylinder"), FLinearColor(0.3f, 0.55f, 0.25f), true); Count = 9; break;
    case ESEBiome::Frozen:    A = Make(TEXT("Cone"), FLinearColor(0.75f, 0.9f, 1.f), true); Count = 12; break;
    case ESEBiome::Volcanic:  A = Make(TEXT("Cone"), FLinearColor(0.06f, 0.05f, 0.06f), true); Count = 12; break;
    default: Count = 0; break;
    }
    for (int32 i = 0; i < Count; ++i)
    {
        TOptional<FVector> Pos = Place(150.f);
        if (!Pos) continue;
        const FVector L = Pos.GetValue();
        const float S = R.FRandRange(0.8f, 1.6f);
        const FRotator Rot(0.f, R.FRandRange(0.f, 360.f), 0.f);
        switch (P.Biome)
        {
        case ESEBiome::Temperate: case ESEBiome::Ocean:
            A->AddInstance(FTransform(Rot, L + FVector(0, 0, 150.f * S), FVector(0.4f, 0.4f, 3.f * S)));
            B->AddInstance(FTransform(Rot, L + FVector(0, 0, 360.f * S), FVector(2.4f * S, 2.4f * S, 2.0f * S)));
            break;
        case ESEBiome::Desert:   A->AddInstance(FTransform(Rot, L + FVector(0, 0, 120.f * S), FVector(0.6f, 0.6f, 2.5f * S))); break;
        case ESEBiome::Frozen:   A->AddInstance(FTransform(Rot, L + FVector(0, 0, 200.f * S), FVector(1.2f * S, 1.2f * S, 4.f * S))); break;
        case ESEBiome::Volcanic: A->AddInstance(FTransform(Rot, L + FVector(0, 0, 250.f * S), FVector(1.4f * S, 1.4f * S, 5.f * S))); break;
        default: break;
        }
    }
    const int32 RockCount = (P.Biome == ESEBiome::Barren) ? 14 : 5;
    for (int32 i = 0; i < RockCount; ++i)
    {
        TOptional<FVector> Pos = Place(100.f);
        if (!Pos) continue;
        const float S = R.FRandRange(0.8f, 2.2f);
        Rocks->AddInstance(FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), Pos.GetValue() + FVector(0, 0, 25.f * S), FVector(S * 1.2f, S, S * 0.6f)));
    }
}

// ================================================================ populator
ASEPopulator::ASEPopulator()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASEPopulator::Init(ASETerrainStreamer* InTerrain, const FSEPlanetData& InPlanet, int32 InSystem, int32 InPlanetIdx)
{
    Terrain = InTerrain; Planet = InPlanet; SystemIdx = InSystem; PlanetIdx = InPlanetIdx;
    Progression = GetGameInstance()->GetSubsystem<USEProgression>();
    Species = SECreatureGen::Generate(Planet);
    for (const FSESpecies& S : Species) SpeciesIds.Add(S.Id);
    bReady = Terrain && Progression;
}

TArray<FName> ASEPopulator::OresFor() const
{
    switch (Planet.Biome)
    {
    case ESEBiome::Temperate: return { TEXT("iron_ore"), TEXT("carbon"), TEXT("silicon"), TEXT("iron_ore") };
    case ESEBiome::Ocean:     return { TEXT("carbon"), TEXT("silicon"), TEXT("ice"), TEXT("iron_ore") };
    case ESEBiome::Desert:    return { TEXT("silicon"), TEXT("copper_ore"), TEXT("iron_ore"), TEXT("carbon") };
    case ESEBiome::Frozen:    return { TEXT("ice"), TEXT("ice"), TEXT("silicon"), TEXT("uranium") };
    case ESEBiome::Volcanic:  return { TEXT("iron_ore"), TEXT("uranium"), TEXT("copper_ore"), TEXT("carbon") };
    default:                  return { TEXT("iron_ore"), TEXT("copper_ore"), TEXT("silicon"), TEXT("ice") };
    }
}

static FLinearColor OreColor(FName Id)
{
    if (Id == TEXT("iron_ore")) return FLinearColor(0.5f, 0.3f, 0.25f);
    if (Id == TEXT("copper_ore")) return FLinearColor(0.85f, 0.5f, 0.2f);
    if (Id == TEXT("carbon")) return FLinearColor(0.08f, 0.08f, 0.1f);
    if (Id == TEXT("ice")) return FLinearColor(0.7f, 0.9f, 1.f);
    if (Id == TEXT("silicon")) return FLinearColor(0.8f, 0.8f, 0.9f);
    if (Id == TEXT("uranium")) return FLinearColor(0.4f, 0.95f, 0.3f);
    return FLinearColor::White;
}

void ASEPopulator::Populate(FIntPoint C)
{
    TArray<TWeakObjectPtr<AActor>>& List = Content.Add(C);
    UWorld* W = GetWorld();
    FRandomStream R(HashCombine(Planet.Seed, HashCombine(C.X * 73856093, C.Y * 19349663)));
    const float CS = Terrain->ChunkSize, Sea = Terrain->GetSeaLevel();
    FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // decor
    if (ASEChunkDecor* D = W->SpawnActor<ASEChunkDecor>(ASEChunkDecor::StaticClass(), FTransform::Identity, SP)) { D->BuildDecor(C, Planet, Terrain); List.Add(D); }

    // ore deposits
    const TArray<FName> Ores = OresFor();
    const int32 NodeCount = R.FRand() < 0.75f ? R.RandRange(1, 2) : 0;
    for (int32 i = 0; i < NodeCount; ++i)
    {
        const float X = (C.X + R.FRand()) * CS, Y = (C.Y + R.FRand()) * CS, H = Terrain->GetHeightAt(X, Y);
        const FName Item = Ores[R.RandRange(0, Ores.Num() - 1)];
        const int32 Amount = R.RandRange(10, 30);
        const float Scale = R.FRandRange(0.9f, 1.8f);
        const int32 Key = (int32)HashCombine(HashCombine((uint32)Planet.Seed, (uint32)(C.X * 31 + C.Y * 17)), (uint32)i);
        if (H < Sea + 80.f || Progression->IsDepleted(Key)) continue;
        if (ASEResourceNode* N = W->SpawnActor<ASEResourceNode>(ASEResourceNode::StaticClass(), FVector(X, Y, H + 45.f * Scale), FRotator::ZeroRotator, SP))
        {
            N->InitNode(Item, Amount, Key, OreColor(Item), Scale); List.Add(N);
        }
    }

    // ancient ruin
    if (R.FRand() < 0.04f)
    {
        const float X = (C.X + R.FRand()) * CS, Y = (C.Y + R.FRand()) * CS, H = Terrain->GetHeightAt(X, Y);
        if (H > Sea + 200.f)
        {
            if (ASERuin* Ru = W->SpawnActor<ASERuin>(ASERuin::StaticClass(), FVector(X, Y, H), FRotator::ZeroRotator, SP))
            {
                Ru->InitRuin((int32)HashCombine((uint32)Planet.Seed, (uint32)(C.X * 7919 + C.Y * 104729))); List.Add(Ru);
            }
        }
    }

    // creatures (capped overall)
    if (Species.Num() > 0 && R.FRand() < 0.33f)
    {
        int32 Alive = 0; for (TActorIterator<ASECreature> It(W); It; ++It) ++Alive;
        if (Alive < 45)
        {
            float Total = 0.f; static const float Wt[] = { 4.f, 1.f, 1.5f, 1.f };
            auto WeightOf = [&](int32 i) { return Wt[FMath::Min(i, 3)]; };
            for (int32 i = 0; i < Species.Num(); ++i) Total += WeightOf(i);
            float Roll = R.FRandRange(0.f, Total); int32 Pick = 0;
            for (int32 i = 0; i < Species.Num(); ++i) { Roll -= WeightOf(i); if (Roll <= 0.f) { Pick = i; break; } }
            const FSESpecies& S = Species[Pick];
            const float GX = (C.X + R.FRand()) * CS, GY = (C.Y + R.FRand()) * CS;
            const int32 N = S.bPack ? R.RandRange(3, 5) : R.RandRange(1, 2);
            ASECreature* Leader = nullptr;
            for (int32 i = 0; i < N; ++i)
            {
                const float X = GX + R.FRandRange(-400.f, 400.f), Y = GY + R.FRandRange(-400.f, 400.f), H = Terrain->GetHeightAt(X, Y);
                if (H < Sea + 100.f) continue;
                if (ASECreature* Cr = W->SpawnActor<ASECreature>(ASECreature::StaticClass(), FVector(X, Y, H + 120.f * S.Size), FRotator::ZeroRotator, SP))
                {
                    Cr->InitCreature(S, Leader, FVector(GX, GY, H));
                    if (!Leader) Leader = Cr;
                    List.Add(Cr);
                }
            }
        }
    }

    // settlements: one candidate per 5x5 chunk cell, in its centre chunk
    const FIntPoint Cell(FMath::FloorToInt((float)C.X / 5.f), FMath::FloorToInt((float)C.Y / 5.f));
    if (C == FIntPoint(Cell.X * 5 + 2, Cell.Y * 5 + 2))
    {
        const bool bHome = (SystemIdx == 0 && PlanetIdx == 0 && Cell == FIntPoint(0, 0));
        const uint32 H = HashCombine(HashCombine((uint32)Planet.Seed, (uint32)Cell.X * 92821u), (uint32)Cell.Y * 68917u);
        const bool bBiomeOk = Planet.Biome == ESEBiome::Temperate || Planet.Biome == ESEBiome::Ocean || Planet.Biome == ESEBiome::Desert || Planet.Biome == ESEBiome::Frozen;
        if (bHome || (bBiomeOk && (H % 100) < 35))
        {
            const float X = (C.X + 0.5f) * CS, Y = (C.Y + 0.5f) * CS, Hc = Terrain->GetHeightAt(X, Y);
            if (Hc > Sea + 300.f)
            {
                const FName Faction = (H % 3 == 0) ? FName(TEXT("wayfarers")) : ((H % 3 == 1) ? FName(TEXT("verdant")) : FName(TEXT("syndicate")));
                if (ASESettlement* S = W->SpawnActor<ASESettlement>(ASESettlement::StaticClass(), FVector(X, Y, Hc), FRotator::ZeroRotator, SP))
                {
                    S->InitSettlement((int32)H, Faction, FVector(X, Y, Hc), Terrain, Planet.Seed, SpeciesIds, SystemIdx, PlanetIdx); List.Add(S);
                }
            }
        }
    }
}

void ASEPopulator::Clear(FIntPoint C)
{
    if (TArray<TWeakObjectPtr<AActor>>* L = Content.Find(C))
    {
        for (TWeakObjectPtr<AActor>& A : *L) { if (A.IsValid()) A->Destroy(); }
        Content.Remove(C);
    }
}

void ASEPopulator::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bReady) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;
    const FVector L = PC->GetPawn()->GetActorLocation();
    const FIntPoint Center(FMath::FloorToInt(L.X / Terrain->ChunkSize), FMath::FloorToInt(L.Y / Terrain->ChunkSize));

    TArray<FIntPoint> Far;
    for (const TPair<FIntPoint, TArray<TWeakObjectPtr<AActor>>>& E : Content)
    {
        const FIntPoint D = E.Key - Center;
        if (FMath::Max(FMath::Abs(D.X), FMath::Abs(D.Y)) > PopulateRadius + 2) Far.Add(E.Key);
    }
    for (const FIntPoint& F : Far) Clear(F);

    // one new chunk per frame, nearest first
    FIntPoint Best; int32 BestD = INT_MAX;
    for (int32 dy = -PopulateRadius; dy <= PopulateRadius; ++dy)
        for (int32 dx = -PopulateRadius; dx <= PopulateRadius; ++dx)
        {
            const FIntPoint C(Center.X + dx, Center.Y + dy);
            if (Content.Contains(C)) continue;
            if (dx * dx + dy * dy < BestD) { BestD = dx * dx + dy * dy; Best = C; }
        }
    if (BestD != INT_MAX) Populate(Best);
}
