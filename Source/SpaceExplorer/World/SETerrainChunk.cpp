#include "SETerrainChunk.h"
#include "ProceduralMeshComponent.h"
#include "Async/Async.h"
#include "Materials/MaterialInterface.h"

// ---------------------------------------------------------------- terrain math
float SETerrain::Height(float X, float Y, int32 Seed, ESEBiome Biome)
{
    const float Ox = (float)(Seed & 0xFFFF) * 0.37f;
    const float Oy = (float)((Seed >> 16) & 0xFFFF) * 0.61f;

    float Amp = 1.f, Rough = 1.f, Offset = 0.f;
    switch (Biome)
    {
    case ESEBiome::Desert:   Amp = 0.45f; Rough = 0.7f; break;
    case ESEBiome::Frozen:   Amp = 1.0f;  Rough = 0.9f; break;
    case ESEBiome::Volcanic: Amp = 1.5f;  Rough = 1.3f; break;
    case ESEBiome::Ocean:    Amp = 0.8f;  Rough = 0.8f; Offset = -9000.f; break;
    case ESEBiome::Barren:   Amp = 0.8f;  Rough = 1.1f; break;
    default: break;
    }

    const float F = 0.00004f;
    const float Cont = FMath::PerlinNoise2D(FVector2D(X * F * 0.35f + Ox, Y * F * 0.35f + Oy));
    const float Mask = FMath::Clamp(Cont * 1.6f, 0.f, 1.f);

    float Ridged = 0.f, Detail = 0.f, A = 1.f, Freq = F, Norm = 0.f;
    for (int32 o = 0; o < 5; ++o)
    {
        const float N = FMath::PerlinNoise2D(FVector2D(X * Freq + Ox + o * 17.1f, Y * Freq + Oy - o * 9.3f));
        const float Rg = 1.f - FMath::Abs(N);
        Ridged += Rg * Rg * A;
        Detail += N * A;
        Norm += A;
        A *= 0.5f; Freq *= 2.1f;
    }
    Ridged /= Norm; Detail /= Norm;

    return Offset + Cont * 6000.f * Amp + Mask * Ridged * 30000.f * Amp + Detail * 900.f * Rough;
}

FLinearColor SETerrain::Color(float Z, float Slope, ESEBiome Biome)
{
    FLinearColor Low, Mid, High, Rock;
    float Sea = -1e9f;
    switch (Biome)
    {
    case ESEBiome::Desert:   Low = {0.78f,0.62f,0.35f}; Mid = {0.85f,0.68f,0.40f}; High = {0.70f,0.45f,0.28f}; Rock = {0.45f,0.32f,0.25f}; break;
    case ESEBiome::Frozen:   Low = {0.80f,0.88f,0.95f}; Mid = {0.90f,0.95f,1.00f}; High = {1.00f,1.00f,1.00f}; Rock = {0.45f,0.5f,0.58f}; break;
    case ESEBiome::Volcanic: Low = {0.12f,0.08f,0.07f}; Mid = {0.22f,0.12f,0.08f}; High = {0.35f,0.15f,0.08f}; Rock = {0.08f,0.07f,0.07f}; break;
    case ESEBiome::Ocean:    Low = {0.86f,0.80f,0.58f}; Mid = {0.20f,0.50f,0.25f}; High = {0.45f,0.45f,0.40f}; Rock = {0.35f,0.33f,0.30f}; Sea = -9000.f; break;
    case ESEBiome::Barren:   Low = {0.45f,0.43f,0.40f}; Mid = {0.55f,0.52f,0.48f}; High = {0.65f,0.62f,0.58f}; Rock = {0.30f,0.28f,0.27f}; break;
    default:                 Low = {0.78f,0.72f,0.50f}; Mid = {0.22f,0.50f,0.18f}; High = {0.85f,0.88f,0.90f}; Rock = {0.40f,0.37f,0.33f}; Sea = -1500.f; break;
    }
    FLinearColor C;
    if (Z < Sea) { C = FLinearColor(0.05f, 0.18f, 0.28f); }  // seabed tint until real water arrives
    else
    {
        const float T = FMath::Clamp((Z + 3000.f) / 33000.f, 0.f, 1.f);
        C = (T < 0.35f) ? FMath::Lerp(Low, Mid, T / 0.35f) : FMath::Lerp(Mid, High, (T - 0.35f) / 0.65f);
    }
    C = FMath::Lerp(C, Rock, FMath::Clamp((Slope - 0.18f) * 3.f, 0.f, 1.f));
    C.A = 1.f;
    return C;
}

// ---------------------------------------------------------------- mesh build
void ASETerrainChunk::BuildMeshData(FIntPoint Coord, float ChunkSize, int32 Res, int32 Seed, ESEBiome Biome, FSETerrainMeshData& Out)
{
    const float Step = ChunkSize / (float)(Res - 1);
    const FVector Sun = FVector(0.45f, 0.30f, 0.84f).GetSafeNormal();
    const float OriginX = Coord.X * ChunkSize;
    const float OriginY = Coord.Y * ChunkSize;

    Out.Vertices.Reserve(Res * Res + 8 * Res);
    Out.Normals.Reserve(Res * Res + 8 * Res);
    Out.Colors.Reserve(Res * Res + 8 * Res);
    Out.UV.Reserve(Res * Res + 8 * Res);

    for (int32 iy = 0; iy < Res; ++iy)
    {
        for (int32 ix = 0; ix < Res; ++ix)
        {
            const float Wx = OriginX + ix * Step;
            const float Wy = OriginY + iy * Step;
            const float H = SETerrain::Height(Wx, Wy, Seed, Biome);
            const float Dx = (SETerrain::Height(Wx + Step, Wy, Seed, Biome) - SETerrain::Height(Wx - Step, Wy, Seed, Biome)) / (2.f * Step);
            const float Dy = (SETerrain::Height(Wx, Wy + Step, Seed, Biome) - SETerrain::Height(Wx, Wy - Step, Seed, Biome)) / (2.f * Step);
            const FVector N = FVector(-Dx, -Dy, 1.f).GetSafeNormal();

            FLinearColor C = SETerrain::Color(H, 1.f - N.Z, Biome);
            const float Light = 0.35f + 0.65f * FMath::Max(0.f, FVector::DotProduct(N, Sun)); // baked lighting
            C.R *= Light; C.G *= Light; C.B *= Light;

            Out.Vertices.Add(FVector(ix * Step, iy * Step, H));
            Out.Normals.Add(N);
            Out.Colors.Add(C);
            Out.UV.Add(FVector2D(Wx / 2000.f, Wy / 2000.f));
        }
    }

    for (int32 iy = 0; iy < Res - 1; ++iy)
    {
        for (int32 ix = 0; ix < Res - 1; ++ix)
        {
            const int32 I = iy * Res + ix;
            Out.Triangles.Append({ I, I + 1, I + Res });
            Out.Triangles.Append({ I + 1, I + 1 + Res, I + Res });
        }
    }

    // Skirts hide cracks between neighbouring chunks of different detail levels.
    const float SkirtDepth = Step * 0.75f + 400.f;
    auto AddSkirt = [&](const TArray<int32>& Edge)
    {
        const int32 Base = Out.Vertices.Num();
        for (int32 Idx : Edge)
        {
            FVector V = Out.Vertices[Idx]; V.Z -= SkirtDepth;
            Out.Vertices.Add(V);
            Out.Normals.Add(Out.Normals[Idx]);
            Out.Colors.Add(Out.Colors[Idx]);
            Out.UV.Add(Out.UV[Idx]);
        }
        for (int32 k = 0; k < Edge.Num() - 1; ++k)
        {
            const int32 A = Edge[k], B = Edge[k + 1], A2 = Base + k, B2 = Base + k + 1;
            Out.Triangles.Append({ A, B, A2 });  Out.Triangles.Append({ B, B2, A2 });   // one winding
            Out.Triangles.Append({ A, A2, B });  Out.Triangles.Append({ B, A2, B2 });   // and the other (two-sided)
        }
    };
    TArray<int32> E;
    E.Reserve(Res);
    for (int32 ix = 0; ix < Res; ++ix) E.Add(ix);                    AddSkirt(E); E.Reset();
    for (int32 ix = 0; ix < Res; ++ix) E.Add((Res - 1) * Res + ix);  AddSkirt(E); E.Reset();
    for (int32 iy = 0; iy < Res; ++iy) E.Add(iy * Res);              AddSkirt(E); E.Reset();
    for (int32 iy = 0; iy < Res; ++iy) E.Add(iy * Res + Res - 1);    AddSkirt(E);
}

// ---------------------------------------------------------------- actor
ASETerrainChunk::ASETerrainChunk()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
    Mesh->bUseAsyncCooking = true;
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->bCastDynamicShadow = false;
    RootComponent = Mesh;
}

void ASETerrainChunk::Activate(FIntPoint InCoord, float InChunkSize, int32 InResolution, bool bInCollision, int32 InSeed, ESEBiome InBiome, UMaterialInterface* InMaterial)
{
    Coord = InCoord;
    Resolution = InResolution;
    bCollision = bInCollision;
    Material = InMaterial;
    bActive = true;
    bReady = false;
    const uint32 Gen = ++Generation;
    SetActorLocation(FVector(Coord.X * InChunkSize, Coord.Y * InChunkSize, 0.f));

    TWeakObjectPtr<ASETerrainChunk> Weak(this);
    Async(EAsyncExecution::ThreadPool, [Weak, Gen, InCoord, InChunkSize, InResolution, InSeed, InBiome]()
    {
        TSharedPtr<FSETerrainMeshData, ESPMode::ThreadSafe> Data = MakeShared<FSETerrainMeshData, ESPMode::ThreadSafe>();
        BuildMeshData(InCoord, InChunkSize, InResolution, InSeed, InBiome, *Data);
        AsyncTask(ENamedThreads::GameThread, [Weak, Gen, Data]()
        {
            if (ASETerrainChunk* Chunk = Weak.Get()) { Chunk->ApplyMesh(Gen, Data); }
        });
    });
}

void ASETerrainChunk::ApplyMesh(uint32 Gen, TSharedPtr<FSETerrainMeshData, ESPMode::ThreadSafe> Data)
{
    if (Gen != Generation || !bActive || !Data.IsValid()) return; // stale result
    Mesh->ClearMeshSection(0);
    Mesh->CreateMeshSection_LinearColor(0, Data->Vertices, Data->Triangles, Data->Normals, Data->UV, Data->Colors, TArray<FProcMeshTangent>(), bCollision);
    if (Material) Mesh->SetMaterial(0, Material);
    SetActorHiddenInGame(false);
    bReady = true;
}

void ASETerrainChunk::Deactivate()
{
    ++Generation;          // invalidates any in-flight build
    bActive = false;
    bReady = false;
    Mesh->ClearMeshSection(0);
    SetActorHiddenInGame(true);
}
