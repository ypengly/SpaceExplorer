#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SEGalaxyTypes.h"
#include "SETerrainChunk.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

namespace SETerrain
{
    /** Terrain height (cm) at a world XY position. Pure and thread-safe. */
    SPACEEXPLORER_API float Height(float X, float Y, int32 Seed, ESEBiome Biome);
    SPACEEXPLORER_API FLinearColor Color(float Z, float Slope, ESEBiome Biome);
}

struct FSETerrainMeshData
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
};

/** A pooled square patch of terrain, built on a worker thread and applied on the game thread. */
UCLASS()
class SPACEEXPLORER_API ASETerrainChunk : public AActor
{
    GENERATED_BODY()
public:
    ASETerrainChunk();

    /** Starts (or restarts) building this chunk. The old mesh stays visible until the new one is ready. */
    void Activate(FIntPoint InCoord, float InChunkSize, int32 InResolution, bool bInCollision, int32 InSeed, ESEBiome InBiome, UMaterialInterface* InMaterial);
    /** Returns the chunk to the pool: hides it and drops its mesh. */
    void Deactivate();

    bool IsReady() const { return bReady; }
    bool HasCollision() const { return bReady && bCollision; }
    int32 GetResolution() const { return Resolution; }
    bool WantsCollision() const { return bCollision; }

    static void BuildMeshData(FIntPoint Coord, float ChunkSize, int32 Res, int32 Seed, ESEBiome Biome, FSETerrainMeshData& Out);

private:
    void ApplyMesh(uint32 Gen, TSharedPtr<FSETerrainMeshData, ESPMode::ThreadSafe> Data);

    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> Material;
    FIntPoint Coord = FIntPoint::ZeroValue;
    int32 Resolution = 0;
    bool bCollision = false;
    bool bReady = false;
    bool bActive = false;
    uint32 Generation = 0;
};
