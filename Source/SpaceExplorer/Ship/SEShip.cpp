#include "SEShip.h"
#include "SEPlayerCharacter.h"
#include "SEInventoryComponent.h"
#include "SEVitalsComponent.h"
#include "SETerrainStreamer.h"
#include "SEProgression.h"
#include "SEWorldObjects.h"
#include "SEVisual.h"
#include "SpaceExplorer.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ASEShip::ASEShip()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;

    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
    Collision->SetBoxExtent(FVector(200.f, 100.f, 50.f));
    Collision->SetCollisionProfileName(TEXT("BlockAll"));
    RootComponent = Collision;

    HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
    HullMesh->SetupAttachment(Collision);
    HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HullMesh->SetRelativeScale3D(FVector(4.f, 2.f, 1.f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) { HullMesh->SetStaticMesh(Cube.Object); }

    CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
    CockpitCamera->SetupAttachment(Collision);
    CockpitCamera->SetRelativeLocation(FVector(140.f, 0.f, 35.f));

    ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
    ChaseArm->SetupAttachment(Collision);
    ChaseArm->TargetArmLength = 900.f;
    ChaseArm->SocketOffset = FVector(0.f, 0.f, 180.f);
    ChaseArm->bEnableCameraLag = true;
    ChaseArm->CameraLagSpeed = 8.f;
    ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
    ChaseCamera->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);
    ChaseCamera->SetAutoActivate(false);

    Cargo = CreateDefaultSubobject<USEInventoryComponent>(TEXT("Cargo"));
}

void ASEShip::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ASETerrainStreamer> It(GetWorld()); It; ++It) { Streamer = *It; break; }
    SyncUpgradesFromTech();
    SEVisual::Colorize(HullMesh, FLinearColor(0.6f, 0.65f, 0.72f));
    Fuel = MaxFuel; Shield = MaxShield; Hull = MaxHull; Energy = MaxEnergy;
    CockpitCamera->SetActive(true);
    ChaseCamera->SetActive(false);
}

FText ASEShip::GetInteractPrompt() const
{
    return bDestroyed ? FText::FromString(TEXT("Wrecked")) : FText::FromString(TEXT("Press F to enter ship"));
}

void ASEShip::SetUpgradeLevel(FName UpgradeId, int32 Level)
{
    UpgradeLevels.Add(UpgradeId, FMath::Clamp(Level, 0, 10));
    RecalculateStats();
}

void ASEShip::SyncUpgradesFromTech()
{
    if (USEProgression* P = GetGameInstance()->GetSubsystem<USEProgression>())
    {
        UpgradeLevels.Add(TEXT("Engine"), P->TechLevel(TEXT("engine_")));
        UpgradeLevels.Add(TEXT("FuelTank"), P->TechLevel(TEXT("fuel_")));
        UpgradeLevels.Add(TEXT("Shield"), P->TechLevel(TEXT("shield_")));
        UpgradeLevels.Add(TEXT("Cargo"), P->TechLevel(TEXT("cargo_")));
    }
    RecalculateStats();
}

void ASEShip::RecalculateStats()
{
    auto Lvl = [this](FName Id) { const int32* L = UpgradeLevels.Find(Id); return L ? *L : 0; };
    MaxSpeed = BaseMaxSpeed * (1.f + 0.12f * Lvl("Engine"));
    MaxFuel = BaseMaxFuel * (1.f + 0.25f * Lvl("FuelTank"));
    MaxShield = BaseMaxShield * (1.f + 0.30f * Lvl("Shield"));
    Cargo->MaxSlots = BaseCargoSlots + 5 * Lvl("Cargo");
    Fuel = FMath::Min(Fuel, MaxFuel);
    Shield = FMath::Min(Shield, MaxShield);
}

void ASEShip::RestoreState(float InFuel, float InShield, float InHull, float InEnergy)
{
    Fuel = FMath::Clamp(InFuel, 0.f, MaxFuel);
    Shield = FMath::Clamp(InShield, 0.f, MaxShield);
    Hull = FMath::Clamp(InHull, 0.f, MaxHull);
    Energy = FMath::Clamp(InEnergy, 0.f, MaxEnergy);
    bDestroyed = Hull <= 0.f;
}

void ASEShip::ApplyShipDamage(float Amount)
{
    if (bDestroyed || Amount <= 0.f) return;
    TimeSinceDamage = 0.f;
    const float Absorbed = FMath::Min(Shield, Amount);
    Shield -= Absorbed;
    Hull = FMath::Max(0.f, Hull - (Amount - Absorbed));
    if (Hull <= 0.f)
    {
        bDestroyed = true;
        UE_LOG(LogSpaceExplorer, Log, TEXT("Ship destroyed."));
        SEVisual::Colorize(HullMesh, FLinearColor(0.05f, 0.05f, 0.06f));
        for (int32 i = 0; i < 14; ++i)
        {
            if (ASEDebris* D = GetWorld()->SpawnActor<ASEDebris>(ASEDebris::StaticClass(), GetActorLocation() + FMath::VRand() * 100.f, FRotator::ZeroRotator))
                D->Launch(FLinearColor(0.2f, 0.2f, 0.25f), FMath::VRand() * 700.f + FVector(0.f, 0.f, 600.f));
        }
        if (Pilot)
        {
            ASEPlayerCharacter* P = Pilot;
            Exit(true);
            P->Vitals->ApplyDamage(50.f);
        }
    }
}

bool ASEShip::Refuel()
{
    if (Fuel >= MaxFuel || !Cargo->RemoveItem(FuelItemId, 1)) return false;
    Fuel = FMath::Min(MaxFuel, Fuel + FuelPerCell);
    return true;
}

void ASEShip::ToggleCameraView()
{
    bCockpitView = !bCockpitView;
    CockpitCamera->SetActive(bCockpitView);
    ChaseCamera->SetActive(!bCockpitView);
}

bool ASEShip::Enter(ASEPlayerCharacter* Player)
{
    if (!Player || Pilot || bDestroyed) return false;
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC) return false;

    Pilot = Player;
    Player->SetActorHiddenInGame(true);
    Player->SetActorEnableCollision(false);
    Player->GetCharacterMovement()->DisableMovement();
    Player->Vitals->bInBreathableAir = true;   // cockpit is pressurised
    PC->Possess(this);
    return true;
}

bool ASEShip::Exit(bool bForce)
{
    if (!Pilot) return false;
    if (!bForce && !bLanded && GetSpeed() > 400.f) return false; // can't leave a moving ship in flight

    ASEPlayerCharacter* P = Pilot;
    APlayerController* PC = Cast<APlayerController>(GetController());
    Pilot = nullptr;
    ThrustIn = StrafeIn = LiftIn = RollIn = YawIn = PitchIn = 0.f;
    bBoostHeld = false;

    const FVector Out = GetActorLocation() + GetActorRightVector() * 330.f + FVector(0.f, 0.f, 120.f);
    P->SetActorLocationAndRotation(Out, FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
    P->SetActorHiddenInGame(false);
    P->SetActorEnableCollision(true);
    P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    P->RefreshAtmosphere();
    if (PC)
    {
        PC->Possess(P);
        PC->SetControlRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
    }
    return true;
}

void ASEShip::SetupPlayerInputComponent(UInputComponent* PIC)
{
    Super::SetupPlayerInputComponent(PIC);
    PIC->BindAxis("MoveForward", this, &ASEShip::InThrust);
    PIC->BindAxis("MoveRight", this, &ASEShip::InStrafe);
    PIC->BindAxis("ShipLift", this, &ASEShip::InLift);
    PIC->BindAxis("ShipRoll", this, &ASEShip::InRoll);
    PIC->BindAxis("Turn", this, &ASEShip::InYaw);
    PIC->BindAxis("LookUp", this, &ASEShip::InPitch);
    PIC->BindAction("Sprint", IE_Pressed, this, &ASEShip::BoostPressed);
    PIC->BindAction("Sprint", IE_Released, this, &ASEShip::BoostReleased);
    PIC->BindAction("ToggleCamera", IE_Pressed, this, &ASEShip::ToggleCameraView);
    PIC->BindAction("Interact", IE_Pressed, this, &ASEShip::ExitPressed);
    PIC->BindAction("Refuel", IE_Pressed, this, &ASEShip::RefuelPressed);
}

void ASEShip::Tick(float Dt)
{
    Super::Tick(Dt);

    // Hold still until the terrain under the ship has collision.
    if (Streamer.IsValid() && !Streamer->IsAreaReady(GetActorLocation()))
    {
        Velocity = FVector::ZeroVector;
        return;
    }

    // --- altitude and gravity
    Altitude = Streamer.IsValid() ? GetActorLocation().Z - Streamer->GetHeightAt(GetActorLocation().X, GetActorLocation().Y) : 1.0e9f;
    GravityFactor = 1.f - FMath::SmoothStep(AtmosphereStartAltitude, AtmosphereEndAltitude, Altitude);
    const FVector Gravity(0.f, 0.f, GetWorld()->GetGravityZ() * GravityFactor);

    const bool bPiloted = Pilot != nullptr && !bDestroyed;

    // --- resources
    TimeSinceDamage += Dt;
    if (TimeSinceDamage > ShieldRegenDelay && !bDestroyed) { Shield = FMath::Min(MaxShield, Shield + ShieldRegenPerSecond * Dt); }
    Energy = FMath::Min(MaxEnergy, Energy + EnergyRegenPerSecond * Dt);

    // --- rotation
    if (bPiloted)
    {
        USEProgression* Pr = GetGameInstance()->GetSubsystem<USEProgression>();
        const float Sens = MouseSensitivity * (Pr ? Pr->Settings.MouseSensitivity : 1.f);
        const float PitchSign = (bInvertPitch != (Pr && Pr->Settings.bInvertY)) ? -1.f : 1.f;
        AddActorLocalRotation(FRotator(PitchIn * Sens * PitchSign, YawIn * Sens, RollIn * RollRate * Dt));
        if (GravityFactor > 0.2f && FMath::IsNearlyZero(PitchIn) && FMath::IsNearlyZero(RollIn))
        {
            FRotator R = GetActorRotation();
            R.Pitch = FMath::FInterpTo(R.Pitch, 0.f, Dt, 2.f);
            R.Roll = FMath::FInterpTo(R.Roll, 0.f, Dt, 2.f);
            SetActorRotation(R);
        }
        PitchIn = YawIn = 0.f; // mouse axes are per-frame deltas
    }

    // --- thrust
    FVector Input = bPiloted ? FVector(ThrustIn, StrafeIn, LiftIn) : FVector::ZeroVector;
    const bool bThrusting = !Input.IsNearlyZero() && Fuel > 0.f;
    bBoosting = false;
    float SpeedCap = MaxSpeed * FMath::Lerp(SpaceSpeedMultiplier, 1.f, GravityFactor);
    if (bThrusting && bBoostHeld && Energy > 1.f)
    {
        bBoosting = true;
        Energy = FMath::Max(0.f, Energy - BoostEnergyPerSecond * Dt);
        SpeedCap *= BoostMultiplier;
    }
    const FVector DesiredWorld = bThrusting ? GetActorQuat().RotateVector(Input.GetClampedToMaxSize(1.f)) * SpeedCap : FVector::ZeroVector;
    if (bThrusting) { Fuel = FMath::Max(0.f, Fuel - FuelPerSecond * (bBoosting ? 3.f : 1.f) * Dt); }

    // Flight assist: velocity eases toward the commanded velocity (zero when idle), so the ship hovers and brakes.
    const float Accel = Acceleration * FMath::Lerp(SpaceSpeedMultiplier * 0.5f, 1.f, GravityFactor) * (bBoosting ? BoostMultiplier : 1.f);
    Velocity = FMath::VInterpConstantTo(Velocity, DesiredWorld, Dt, Accel);
    Velocity += Gravity * Dt;

    // --- move with collision
    bLanded = false;
    FHitResult Hit;
    AddActorWorldOffset(Velocity * Dt, true, &Hit);
    if (Hit.bBlockingHit)
    {
        const float ImpactSpeed = -FVector::DotProduct(Velocity, Hit.ImpactNormal);
        if (ImpactSpeed > ImpactDamageThreshold) { ApplyShipDamage((ImpactSpeed - ImpactDamageThreshold) * 0.03f); }
        Velocity = FVector::VectorPlaneProject(Velocity, Hit.ImpactNormal);
        if (Hit.ImpactNormal.Z > 0.6f) { bLanded = true; }
    }
}
