#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SEGalaxyTypes.h"
#include "SEPlayerCharacter.generated.h"

class UCameraComponent;
class USEVitalsComponent;
class USEInventoryComponent;
class ASETerrainStreamer;
class ASEBaseStructure;
class USEProgression;

/** First-person explorer: movement, multitool (mine/fight), scanner, interaction, base building, hazards. */
UCLASS()
class SPACEEXPLORER_API ASEPlayerCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ASEPlayerCharacter();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USEVitalsComponent> Vitals;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USEInventoryComponent> Inventory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float WalkSpeed = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float SprintSpeed = 800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float CrouchSpeed = 220.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float SprintEnergyPerSec = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") float InteractRange = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool") float ToolRange = 4500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool") float ToolEnergyCost = 4.f;

    void RefreshAtmosphere();
    void HoldForTerrain();
    void ApplySettings();
    bool IsHoldingForTerrain() const { return bWaitingForTerrain; }
    bool IsNearFabricator() const { return bNearFabricator; }
    bool IsExposed() const { return bExposed; }
    bool IsInWater() const { return bInWater; }

    AActor* GetFocusedActor() const { return FocusActor.Get(); }
    FText GetFocusPrompt() const;

    // Base building
    bool IsBuildMode() const { return bBuildMode; }
    FString GetBuildText() const;
    TArray<FName> GetBuildableStructures() const;

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
    virtual void BeginPlay() override;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void StartSprint();
    void StopSprint();
    void ToggleCrouch();
    void InteractPressed();
    void ToolPressed();
    void ToolReleased() { bFiring = false; }
    void ScanPressed();
    void BuildTogglePressed();
    void RotateBuildPressed();
    void BuildNext();
    void BuildPrev();
    void UpdateFocus();
    void FireTool();
    void PlaceStructure();
    void UpdateGhost();
    void UpdateZones();
    UFUNCTION() void HandleDeath();

    TWeakObjectPtr<ASETerrainStreamer> Streamer;
    TWeakObjectPtr<AActor> FocusActor;
    UPROPERTY() TObjectPtr<USEProgression> Progression;
    UPROPERTY() TObjectPtr<ASEBaseStructure> Ghost;
    FSEPlanetData Planet;
    bool bWantsSprint = false, bWaitingForTerrain = false, bFiring = false;
    bool bBuildMode = false, bNearFabricator = false, bExposed = false, bInWater = false;
    float ToolCooldown = 0.f, ScanCooldown = 0.f, ZoneTimer = 0.f, MissionTimer = 0.f, BuildYaw = 0.f;
    int32 BuildIndex = 0;
    bool bCanAffordBuild = false;
    FVector BuildLocation = FVector::ZeroVector;
    bool bHasBuildSpot = false;
};
