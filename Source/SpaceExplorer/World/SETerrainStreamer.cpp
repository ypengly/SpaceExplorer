#include "SETerrainStreamer.h"
#include "SETerrainChunk.h"
#include "SpaceExplorer.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"

ASETerrainStreamer::ASETerrainStreamer()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASETerrainStreamer::InitForPlanet(const FSEPlanetData& Planet)
{
    PlanetData = Planet;
    bInitialised = true;

    if (AWorldSettings* WS = GetWorld()->GetWorldSettings())
    {
        WS->bWorldGravitySet = true;
        WS->WorldGravityZ = -980.f * Planet.Gravity;
    }

    Material = nullptr;
    if (TerrainMaterialPath.IsValid())
    {
        Material = Cast<UMaterialInterface>(TerrainMaterialPath.TryLoad());
    }
    if (!Material)
    {
        Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
        if (!Material) UE_LOG(LogSpaceExplorer, Warning, TEXT("No terrain material found; terrain will use the default material."));
    }
    UE_LOG(LogSpaceExplorer, Log, TEXT("Terrain streamer ready for planet '%s' (gravity %.2fg)"), *Planet.Name, Planet.Gravity);
}

float ASETerrainStreamer::GetHeightAt(float X, float Y) const
{
    return SETerrain::Height(X, Y, PlanetData.Seed, PlanetData.Biome);
}

float ASETerrainStreamer::GetSeaLevel() const
{
    switch (PlanetData.Biome)
    {
    case ESEBiome::Ocean: return -9000.f;
    case ESEBiome::Temperate: return -1500.f;
    default: return -1.0e9f;
    }
}

FVector2D ASETerrainStreamer::FindDrySpot(FVector2D From, float MinRadius) const
{
    const float Sea = GetSeaLevel();
    for (float Rad = MinRadius; Rad <= 400000.f; Rad += 2000.f)
    {
        const int32 Steps = Rad < 1.f ? 1 : 12;
        for (int32 a = 0; a < Steps; ++a)
        {
            const float Ang = a * TWO_PI / 12.f;
            const FVector2D P = From + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Rad;
            const float H = GetHeightAt(P.X, P.Y);
            if (H < Sea + 600.f) continue;
            const float Slope = FMath::Abs(GetHeightAt(P.X + 500.f, P.Y) - H) + FMath::Abs(GetHeightAt(P.X, P.Y + 500.f) - H);
            if (Slope < 700.f) return P;
        }
    }
    return From;
}

int32 ASETerrainStreamer::ResolutionForDistance(int32 D) const
{
    if (D <= 2) return 33;
    if (D <= 5) return 17;
    return 9;
}

ASETerrainChunk* ASETerrainStreamer::AcquireChunk()
{
    if (Pool.Num() > 0) { return Pool.Pop(); }
    FActorSpawnParameters P;
    P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return GetWorld()->SpawnActor<ASETerrainChunk>(ASETerrainChunk::StaticClass(), FTransform::Identity, P);
}

void ASETerrainStreamer::ReleaseChunk(ASETerrainChunk* Chunk)
{
    if (!Chunk) return;
    Chunk->Deactivate();
    Pool.Add(Chunk);
}

bool ASETerrainStreamer::IsAreaReady(const FVector& Loc, int32 Radius) const
{
    const FIntPoint C(FMath::FloorToInt(Loc.X / ChunkSize), FMath::FloorToInt(Loc.Y / ChunkSize));
    for (int32 dy = -Radius; dy <= Radius; ++dy)
    {
        for (int32 dx = -Radius; dx <= Radius; ++dx)
        {
            const TObjectPtr<ASETerrainChunk>* Found = Active.Find(FIntPoint(C.X + dx, C.Y + dy));
            if (!Found || !(*Found) || !(*Found)->HasCollision()) return false;
        }
    }
    return true;
}

void ASETerrainStreamer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bInitialised) return;

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    const AActor* Focus = PC ? PC->GetPawn() : nullptr;
    const FVector Loc = Focus ? Focus->GetActorLocation() : FVector::ZeroVector;
    const FIntPoint Center(FMath::FloorToInt(Loc.X / ChunkSize), FMath::FloorToInt(Loc.Y / ChunkSize));

    // Unload chunks that left the view radius.
    for (auto It = Active.CreateIterator(); It; ++It)
    {
        const FIntPoint D = It.Key() - Center;
        if (FMath::Max(FMath::Abs(D.X), FMath::Abs(D.Y)) > ViewRadius + 1)
        {
            ReleaseChunk(It.Value());
            It.RemoveCurrent();
        }
    }

    // Collect wanted chunks, nearest first.
    struct FWanted { FIntPoint Coord; int32 Dist2; int32 Cheb; };
    TArray<FWanted> Wanted;
    for (int32 dy = -ViewRadius; dy <= ViewRadius; ++dy)
        for (int32 dx = -ViewRadius; dx <= ViewRadius; ++dx)
            Wanted.Add({ FIntPoint(Center.X + dx, Center.Y + dy), dx * dx + dy * dy, FMath::Max(FMath::Abs(dx), FMath::Abs(dy)) });
    Wanted.Sort([](const FWanted& A, const FWanted& B) { return A.Dist2 < B.Dist2; });

    int32 Starts = 0;
    for (const FWanted& W : Wanted)
    {
        if (Starts >= MaxStartsPerFrame) break;
        const int32 Res = ResolutionForDistance(W.Cheb);
        const bool bCol = W.Cheb <= CollisionRadius;

        if (TObjectPtr<ASETerrainChunk>* Found = Active.Find(W.Coord))
        {
            ASETerrainChunk* C = *Found;
            if (C && C->IsReady() && (C->GetResolution() != Res || C->WantsCollision() != bCol))
            {
                C->Activate(W.Coord, ChunkSize, Res, bCol, PlanetData.Seed, PlanetData.Biome, Material);
                ++Starts;
            }
        }
        else if (ASETerrainChunk* NewChunk = AcquireChunk())
        {
            NewChunk->Activate(W.Coord, ChunkSize, Res, bCol, PlanetData.Seed, PlanetData.Biome, Material);
            Active.Add(W.Coord, NewChunk);
            ++Starts;
        }
    }
}
