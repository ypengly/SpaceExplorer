#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SEInteractable.h"
#include "SEShip.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UCameraComponent;
class USpringArmComponent;
class USEInventoryComponent;
class ASEPlayerCharacter;
class ASETerrainStreamer;

/**
 * Player spaceship: six-degree-of-freedom flight with flight assist, boost, fuel, energy,
 * shields, hull, cargo, upgrades, cockpit/chase cameras and enter/exit.
 * Gravity fades out with altitude, so the same ship lands on a planet and flies in space.
 */
UCLASS()
class SPACEEXPLORER_API ASEShip : public APawn, public ISEInteractable
{
    GENERATED_BODY()
public:
    ASEShip();

    // --- Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UBoxComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> HullMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> CockpitCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USpringArmComponent> ChaseArm;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> ChaseCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USEInventoryComponent> Cargo;

    // --- Base stats (before upgrades)
    UPROPERTY(EditAnywhere, Category="Ship|Base") float BaseMaxSpeed = 9000.f;      // cm/s in atmosphere
    UPROPERTY(EditAnywhere, Category="Ship|Base") float BaseMaxFuel = 100.f;
    UPROPERTY(EditAnywhere, Category="Ship|Base") float BaseMaxShield = 100.f;
    UPROPERTY(EditAnywhere, Category="Ship|Base") int32 BaseCargoSlots = 20;
    UPROPERTY(EditAnywhere, Category="Ship") float MaxHull = 100.f;
    UPROPERTY(EditAnywhere, Category="Ship") float MaxEnergy = 100.f;
    UPROPERTY(EditAnywhere, Category="Ship") float Acceleration = 4500.f;
    UPROPERTY(EditAnywhere, Category="Ship") float BoostMultiplier = 2.5f;
    UPROPERTY(EditAnywhere, Category="Ship") float SpaceSpeedMultiplier = 25.f;
    UPROPERTY(EditAnywhere, Category="Ship") float MouseSensitivity = 1.2f;
    UPROPERTY(EditAnywhere, Category="Ship") float RollRate = 90.f;
    UPROPERTY(EditAnywhere, Category="Ship") bool bInvertPitch = false;
    UPROPERTY(EditAnywhere, Category="Ship") float FuelPerSecond = 0.6f;
    UPROPERTY(EditAnywhere, Category="Ship") float BoostEnergyPerSecond = 18.f;
    UPROPERTY(EditAnywhere, Category="Ship") float EnergyRegenPerSecond = 10.f;
    UPROPERTY(EditAnywhere, Category="Ship") float ShieldRegenPerSecond = 8.f;
    UPROPERTY(EditAnywhere, Category="Ship") float ShieldRegenDelay = 4.f;
    UPROPERTY(EditAnywhere, Category="Ship") float ImpactDamageThreshold = 900.f;
    UPROPERTY(EditAnywhere, Category="Ship") float AtmosphereStartAltitude = 60000.f;
    UPROPERTY(EditAnywhere, Category="Ship") float AtmosphereEndAltitude = 200000.f;
    UPROPERTY(EditAnywhere, Category="Ship") float FuelPerCell = 25.f;
    UPROPERTY(EditAnywhere, Category="Ship") FName FuelItemId = TEXT("fuel_cell");

    // --- Live state
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") float Fuel = 100.f;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") float Shield = 100.f;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") float Hull = 100.f;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") float Energy = 100.f;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") float MaxFuel = 100.f;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") float MaxShield = 100.f;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") float MaxSpeed = 9000.f;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") bool bLanded = false;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") bool bBoosting = false;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") bool bDestroyed = false;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") bool bCockpitView = true;
    UPROPERTY(BlueprintReadOnly, Category="Ship|State") float Altitude = 0.f;

    /** Upgrade id -> level. Ids: Engine, FuelTank, Shield, Cargo. */
    UPROPERTY() TMap<FName, int32> UpgradeLevels;

    UFUNCTION(BlueprintCallable, Category="Ship") void SetUpgradeLevel(FName UpgradeId, int32 Level);
    UFUNCTION(BlueprintCallable, Category="Ship") void RecalculateStats();
    UFUNCTION(BlueprintCallable, Category="Ship") void ApplyShipDamage(float Amount);
    UFUNCTION(BlueprintCallable, Category="Ship") bool Refuel();
    /** Reads engine/fuel/shield/cargo upgrade levels from unlocked technology. */
    void SyncUpgradesFromTech();
    bool IsInSpace() const { return Altitude >= AtmosphereEndAltitude; }
    void RepairHull(float Amount) { Hull = FMath::Min(MaxHull, Hull + Amount); if (Hull > 0.f) bDestroyed = false; }
    UFUNCTION(BlueprintCallable, Category="Ship") void ToggleCameraView();
    UFUNCTION(BlueprintPure, Category="Ship") float GetSpeed() const { return Velocity.Size(); }
    UFUNCTION(BlueprintPure, Category="Ship") bool HasPilot() const { return Pilot != nullptr; }

    bool Enter(ASEPlayerCharacter* Player);
    bool Exit(bool bForce = false);
    ASEPlayerCharacter* GetPilot() const { return Pilot; }

    // ISEInteractable
    virtual void Interact(ASEPlayerCharacter* Interactor) override { Enter(Interactor); }
    virtual FText GetInteractPrompt() const override;

    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    /** Restores saved state without side effects. */
    void RestoreState(float InFuel, float InShield, float InHull, float InEnergy);

protected:
    virtual void BeginPlay() override;

private:
    void InThrust(float V) { ThrustIn = V; }
    void InStrafe(float V) { StrafeIn = V; }
    void InLift(float V)   { LiftIn = V; }
    void InRoll(float V)   { RollIn = V; }
    void InYaw(float V)    { YawIn = V; }
    void InPitch(float V)  { PitchIn = V; }
    void BoostPressed()    { bBoostHeld = true; }
    void BoostReleased()   { bBoostHeld = false; }
    void ExitPressed()     { Exit(false); }
    void RefuelPressed()   { Refuel(); }

    UPROPERTY() TObjectPtr<ASEPlayerCharacter> Pilot;
    TWeakObjectPtr<ASETerrainStreamer> Streamer;
    FVector Velocity = FVector::ZeroVector;
    float ThrustIn = 0.f, StrafeIn = 0.f, LiftIn = 0.f, RollIn = 0.f, YawIn = 0.f, PitchIn = 0.f;
    bool bBoostHeld = false;
    float TimeSinceDamage = 100.f;
    float GravityFactor = 1.f;
};
