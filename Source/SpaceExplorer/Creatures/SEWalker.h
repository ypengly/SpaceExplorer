#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SEWalker.generated.h"

class ASETerrainStreamer;
class UStaticMeshComponent;

/**
 * Base for creatures and NPCs: waits for terrain collision, moves toward targets without a
 * navmesh, and throttles itself by distance from the player (distance-based AI).
 */
UCLASS(Abstract)
class SPACEEXPLORER_API ASEWalker : public ACharacter
{
    GENERATED_BODY()
public:
    ASEWalker();
    virtual void Tick(float Dt) override;
    float GetDistToPlayer() const { return DistToPlayer; }

protected:
    virtual void BeginPlay() override;
    virtual void Think(float Dt) {}
    /** Returns true when within Accept of the target (horizontal distance). */
    bool MoveToward(const FVector& Target, float SpeedScale = 1.f, float Accept = 120.f);

    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(EditAnywhere, Category="Walker") float WalkSpeed = 300.f;
    TWeakObjectPtr<ASETerrainStreamer> Streamer;
    float DistToPlayer = 0.f;
    float DistTimer = 0.f;
    bool bWaitingTerrain = true;
};
