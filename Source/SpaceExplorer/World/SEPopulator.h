#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SEGalaxyTypes.h"
#include "SECreature.h"
#include "SEPopulator.generated.h"

class ASETerrainStreamer;
class USEProgression;
class UInstancedStaticMeshComponent;

/** Foliage / rocks for one chunk, rendered with instanced meshes (one draw call per kind). */
UCLASS()
class SPACEEXPLORER_API ASEChunkDecor : public AActor
{
    GENERATED_BODY()
public:
    ASEChunkDecor();
    void BuildDecor(FIntPoint Coord, const FSEPlanetData& Planet, ASETerrainStreamer* Terrain);
};

/**
 * Deterministically fills chunks near the player with decor, ore deposits, creatures, ruins and
 * settlements, and removes them again when the player moves away (distance-based streaming).
 */
UCLASS()
class SPACEEXPLORER_API ASEPopulator : public AActor
{
    GENERATED_BODY()
public:
    ASEPopulator();
    void Init(ASETerrainStreamer* InTerrain, const FSEPlanetData& InPlanet, int32 InSystem, int32 InPlanetIdx);
    virtual void Tick(float Dt) override;

    int32 PopulateRadius = 3;
private:
    void Populate(FIntPoint Coord);
    void Clear(FIntPoint Coord);
    TArray<FName> OresFor() const;

    UPROPERTY() TObjectPtr<ASETerrainStreamer> Terrain;
    UPROPERTY() TObjectPtr<USEProgression> Progression;
    TMap<FIntPoint, TArray<TWeakObjectPtr<AActor>>> Content;
    FSEPlanetData Planet;
    int32 SystemIdx = 0, PlanetIdx = 0;
    TArray<FSESpecies> Species;
    TArray<FName> SpeciesIds;
    bool bReady = false;
};
