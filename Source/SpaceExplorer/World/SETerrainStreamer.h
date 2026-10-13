#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SEGalaxyTypes.h"
#include "SETerrainStreamer.generated.h"

class ASETerrainChunk;
class UMaterialInterface;

/**
 * Streams terrain chunks around the player: object pool, level-of-detail by distance,
 * collision only near the player, async mesh generation and a per-frame start budget.
 * Also applies the current planet's gravity to the world.
 */
UCLASS(Config=Game)
class SPACEEXPLORER_API ASETerrainStreamer : public AActor
{
    GENERATED_BODY()
public:
    ASETerrainStreamer();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain") int32 ViewRadius = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain") float ChunkSize = 6400.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain") int32 MaxStartsPerFrame = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain") int32 CollisionRadius = 3;
    /** Optional lit material using vertex colour. If empty, an engine debug material is used (lighting is baked into vertex colours). */
    UPROPERTY(Config, EditAnywhere, Category="Terrain") FSoftObjectPath TerrainMaterialPath;

    void InitForPlanet(const FSEPlanetData& Planet);
    float GetHeightAt(float X, float Y) const;
    /** Water surface height, or a huge negative number on planets without oceans. */
    float GetSeaLevel() const;
    /** Finds nearby gentle, dry ground, searching outward from From (starting at MinRadius). */
    FVector2D FindDrySpot(FVector2D From, float MinRadius = 0.f) const;
    /** True when the chunk under Loc (and its neighbours within Radius) has finished building with collision. */
    bool IsAreaReady(const FVector& Loc, int32 Radius = 0) const;
    int32 GetActiveChunkCount() const { return Active.Num(); }
    const FSEPlanetData& GetPlanet() const { return PlanetData; }

    virtual void Tick(float DeltaSeconds) override;

private:
    int32 ResolutionForDistance(int32 ChunkDist) const;
    ASETerrainChunk* AcquireChunk();
    void ReleaseChunk(ASETerrainChunk* Chunk);

    UPROPERTY() TMap<FIntPoint, TObjectPtr<ASETerrainChunk>> Active;
    UPROPERTY() TArray<TObjectPtr<ASETerrainChunk>> Pool;
    UPROPERTY() TObjectPtr<UMaterialInterface> Material;
    FSEPlanetData PlanetData;
    bool bInitialised = false;
};
