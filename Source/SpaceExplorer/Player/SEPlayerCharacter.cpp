#include "SEPlayerCharacter.h"
#include "SpaceExplorer.h"
#include "SEVitalsComponent.h"
#include "SEInventoryComponent.h"
#include "SEInteractable.h"
#include "SETerrainStreamer.h"
#include "SEGalaxySubsystem.h"
#include "SEProgression.h"
#include "SEGameData.h"
#include "SEWorldObjects.h"
#include "SECreature.h"
#include "SENPC.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

ASEPlayerCharacter::ASEPlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
    FirstPersonCamera->bUsePawnControlRotation = true;

    Vitals = CreateDefaultSubobject<USEVitalsComponent>(TEXT("Vitals"));
    Inventory = CreateDefaultSubobject<USEInventoryComponent>(TEXT("Inventory"));

    bUseControllerRotationYaw = true;
    UCharacterMovementComponent* Move = GetCharacterMovement();
    Move->NavAgentProps.bCanCrouch = true;
    Move->MaxWalkSpeed = WalkSpeed;
    Move->MaxWalkSpeedCrouched = CrouchSpeed;
    Move->JumpZVelocity = 520.f;
    Move->AirControl = 0.4f;
}

void ASEPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
    Vitals->OnPlayerDied.AddDynamic(this, &ASEPlayerCharacter::HandleDeath);
    Progression = GetGameInstance()->GetSubsystem<USEProgression>();
    for (TActorIterator<ASETerrainStreamer> It(GetWorld()); It; ++It) { Streamer = *It; break; }
    RefreshAtmosphere();
    ApplySettings();

    if (Streamer.IsValid())
    {
        const FVector2D Spot = Streamer->FindDrySpot(FVector2D::ZeroVector, 0.f);
        SetActorLocation(FVector(Spot.X, Spot.Y, Streamer->GetHeightAt(Spot.X, Spot.Y) + 150.f), false, nullptr, ETeleportType::TeleportPhysics);
        HoldForTerrain();
    }
}

void ASEPlayerCharacter::ApplySettings()
{
    if (Progression) FirstPersonCamera->SetFieldOfView(Progression->Settings.FOV);
}

void ASEPlayerCharacter::RefreshAtmosphere()
{
    if (USEGalaxySubsystem* G = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>())
    {
        Planet = G->GetCurrentPlanet();
        Vitals->bInBreathableAir = Planet.bBreathable;
    }
}

void ASEPlayerCharacter::HoldForTerrain()
{
    if (!Streamer.IsValid()) return;
    bWaitingForTerrain = true;
    GetCharacterMovement()->SetMovementMode(MOVE_None);
}

FText ASEPlayerCharacter::GetFocusPrompt() const
{
    if (const AActor* A = FocusActor.Get())
        if (const ISEInteractable* I = Cast<ISEInteractable>(A)) return I->GetInteractPrompt();
    return FText::GetEmpty();
}

void ASEPlayerCharacter::UpdateFocus()
{
    FocusActor = nullptr;
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + FirstPersonCamera->GetForwardVector() * InteractRange;
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SEInteract), false, this);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Q))
    {
        AActor* A = Hit.GetActor();
        if (A && A->Implements<USEInteractable>()) { FocusActor = A; }
    }
}

// ------------------------------------------------------------------ zones, hazards, water
void ASEPlayerCharacter::UpdateZones()
{
    const FVector Loc = GetActorLocation();
    const ASEBaseStructure::FZone Z = ASEBaseStructure::QueryZone(GetWorld(), Loc);
    Vitals->bSheltered = Z.bShelter;
    bNearFabricator = Z.bFabricator;
    if (Z.bHeal) Vitals->Heal(1.5f);
    if (Z.bSolar) Vitals->RestoreEnergy(6.f);

    Vitals->MaxOxygen = 100.f + (Progression && Progression->HasTech(TEXT("oxygen_tank")) ? 50.f : 0.f);

    const float Sea = Streamer.IsValid() ? Streamer->GetSeaLevel() : -1.0e9f;
    bInWater = (Loc.Z - 96.f) < Sea;
    Vitals->bUnderwater = FirstPersonCamera->GetComponentLocation().Z < Sea;

    const bool bExtreme = Planet.TemperatureC > 75.f || Planet.TemperatureC < -45.f;
    bExposed = bExtreme && !Z.bShelter && !(Progression && Progression->HasTech(TEXT("thermal_protection")));
    if (bExposed) Vitals->ApplyDamage(1.5f);
}

void ASEPlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bWaitingForTerrain)
    {
        if (!Streamer.IsValid() || Streamer->IsAreaReady(GetActorLocation()))
        {
            bWaitingForTerrain = false;
            GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        }
        return;
    }

    UpdateFocus();
    ToolCooldown -= DeltaTime; ScanCooldown -= DeltaTime;
    ZoneTimer += DeltaTime; MissionTimer += DeltaTime;
    if (ZoneTimer >= 0.5f) { ZoneTimer = 0.f; UpdateZones(); }
    if (MissionTimer >= 1.f && Progression)
    {
        MissionTimer = 0.f;
        if (USEGalaxySubsystem* G = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>())
            Progression->CheckLocation(GetActorLocation(), G->CurrentSystem, G->CurrentPlanet);
    }
    if (bFiring && !bBuildMode && ToolCooldown <= 0.f) { FireTool(); }
    if (bBuildMode) UpdateGhost();

    UCharacterMovementComponent* Move = GetCharacterMovement();
    const float WaterMul = bInWater ? 0.55f : 1.f;
    const bool bMoving = GetVelocity().SizeSquared2D() > 25.f;
    if (bWantsSprint && bMoving && !bIsCrouched && Move->IsMovingOnGround())
    {
        if (Vitals->ConsumeEnergy(SprintEnergyPerSec * DeltaTime)) { Move->MaxWalkSpeed = SprintSpeed * WaterMul; return; }
        bWantsSprint = false;
    }
    Move->MaxWalkSpeed = WalkSpeed * WaterMul;
}

// ------------------------------------------------------------------ tool / scan
void ASEPlayerCharacter::ToolPressed()
{
    if (bBuildMode) { PlaceStructure(); return; }
    bFiring = true;
}

void ASEPlayerCharacter::FireTool()
{
    ToolCooldown = 0.25f;
    if (Vitals->IsDead() || !Vitals->ConsumeEnergy(ToolEnergyCost)) return;
    const float Mult = (Progression && Progression->HasTech(TEXT("mining_2"))) ? 1.6f : 1.f;
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + FirstPersonCamera->GetForwardVector() * ToolRange;
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SETool), false, this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Q);
#if !UE_BUILD_SHIPPING
    DrawDebugLine(GetWorld(), Start + FirstPersonCamera->GetRightVector() * 20.f - FVector(0, 0, 20.f), bHit ? Hit.ImpactPoint : End, FColor::Cyan, false, 0.08f, 0, 2.f);
#endif
    if (!bHit) return;
    AActor* A = Hit.GetActor();
    if (ASEResourceNode* N = Cast<ASEResourceNode>(A)) { N->Mine(this, FMath::RoundToInt(4.f * Mult)); }
    else if (ASECreature* C = Cast<ASECreature>(A)) { C->TakeToolDamage(22.f * Mult, this); }
    else if (ASENPC* P = Cast<ASENPC>(A)) { P->TakeToolDamage(10.f, this); }
}

void ASEPlayerCharacter::ScanPressed()
{
    if (ScanCooldown > 0.f || !Progression || Vitals->IsDead()) return;
    ScanCooldown = 0.6f;
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + FirstPersonCamera->GetForwardVector() * 8000.f;
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SEScan), false, this);
    if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(120.f), Q))
    {
        if (const ISEScannable* S = Cast<ISEScannable>(Hit.GetActor()))
        {
            const bool bNew = Progression->AddDiscovery(S->GetDiscoveryId(), S->GetScanName(), S->GetScanInfo(), S->GetScanReward());
            Progression->ProgressMissions(1, NAME_None, 1);
            Progression->PushNotice(bNew ? FString::Printf(TEXT("Discovered: %s  (+%d research)"), *S->GetScanName(), S->GetScanReward())
                                         : FString::Printf(TEXT("Scanned: %s - %s"), *S->GetScanName(), *S->GetScanInfo()));
            return;
        }
    }
    Progression->PushNotice(TEXT("Nothing to scan. Aim at ore, creatures, ruins or settlements."));
}

void ASEPlayerCharacter::InteractPressed()
{
    if (Vitals->IsDead()) return;
    if (AActor* A = FocusActor.Get())
        if (ISEInteractable* I = Cast<ISEInteractable>(A)) { I->Interact(this); }
}

// ------------------------------------------------------------------ building
TArray<FName> ASEPlayerCharacter::GetBuildableStructures() const
{
    TArray<FName> Out;
    if (USEGameData* GD = GetGameInstance()->GetSubsystem<USEGameData>())
        for (const FSEStructureDef& S : GD->Structures) { if (!Progression || Progression->HasTech(S.RequiredTech)) Out.Add(S.Id); }
    return Out;
}

FString ASEPlayerCharacter::GetBuildText() const
{
    USEGameData* GD = GetGameInstance()->GetSubsystem<USEGameData>();
    const TArray<FName> List = GetBuildableStructures();
    if (!GD || List.Num() == 0) return FString();
    const FSEStructureDef* D = GD->FindStructure(List[BuildIndex % List.Num()]);
    if (!D) return FString();
    FString Cost;
    for (const TPair<FName, int32>& C : D->Cost) Cost += FString::Printf(TEXT("%dx %s  "), C.Value, *GD->ItemName(C.Key));
    return FString::Printf(TEXT("BUILD: %s  -  %s\nCost: %s%s\nWheel: change   R: rotate   Left mouse: place   B: exit"), *D->Name, *D->Desc, *Cost, bCanAffordBuild ? TEXT("") : TEXT("(not enough materials)"));
}

void ASEPlayerCharacter::BuildTogglePressed()
{
    bBuildMode = !bBuildMode;
    if (!bBuildMode && Ghost) { Ghost->Destroy(); Ghost = nullptr; }
}
void ASEPlayerCharacter::RotateBuildPressed() { if (bBuildMode) BuildYaw = FMath::Fmod(BuildYaw + 90.f, 360.f); }
void ASEPlayerCharacter::BuildNext() { if (bBuildMode) { ++BuildIndex; if (Ghost) { Ghost->Destroy(); Ghost = nullptr; } } }
void ASEPlayerCharacter::BuildPrev() { if (bBuildMode) { BuildIndex += GetBuildableStructures().Num() - 1; if (Ghost) { Ghost->Destroy(); Ghost = nullptr; } } }

void ASEPlayerCharacter::UpdateGhost()
{
    USEGameData* GD = GetGameInstance()->GetSubsystem<USEGameData>();
    const TArray<FName> List = GetBuildableStructures();
    if (!GD || List.Num() == 0) return;
    const FName Type = List[BuildIndex % List.Num()];
    const FSEStructureDef* D = GD->FindStructure(Type);
    if (!D) return;

    if (!Ghost)
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Ghost = GetWorld()->SpawnActor<ASEBaseStructure>(ASEBaseStructure::StaticClass(), FTransform::Identity, P);
        if (Ghost) Ghost->InitStructure(Type, true);
    }
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + FirstPersonCamera->GetForwardVector() * 1500.f;
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SEBuild), false, this);
    if (Ghost) Q.AddIgnoredActor(Ghost);
    bHasBuildSpot = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Q);
    if (bHasBuildSpot)
    {
        BuildLocation = FVector(FMath::GridSnap(Hit.ImpactPoint.X, 100.f), FMath::GridSnap(Hit.ImpactPoint.Y, 100.f), Hit.ImpactPoint.Z + D->Size.Z * 50.f);
        if (Ghost) Ghost->SetActorLocationAndRotation(BuildLocation, FRotator(0.f, BuildYaw, 0.f));
    }
    bCanAffordBuild = true;
    for (const TPair<FName, int32>& C : D->Cost) { if (!Inventory->HasItem(C.Key, C.Value)) bCanAffordBuild = false; }
    if (Ghost) { Ghost->SetGhostColor(bCanAffordBuild && bHasBuildSpot); Ghost->SetActorHiddenInGame(!bHasBuildSpot); }
}

void ASEPlayerCharacter::PlaceStructure()
{
    USEGameData* GD = GetGameInstance()->GetSubsystem<USEGameData>();
    const TArray<FName> List = GetBuildableStructures();
    if (!GD || !Progression || List.Num() == 0 || !bHasBuildSpot) return;
    const FSEStructureDef* D = GD->FindStructure(List[BuildIndex % List.Num()]);
    if (!D) return;
    for (const TPair<FName, int32>& C : D->Cost)
        if (!Inventory->HasItem(C.Key, C.Value)) { Progression->PushNotice(TEXT("Not enough materials for ") + D->Name); return; }
    for (const TPair<FName, int32>& C : D->Cost) Inventory->RemoveItem(C.Key, C.Value);

    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (ASEBaseStructure* S = GetWorld()->SpawnActor<ASEBaseStructure>(ASEBaseStructure::StaticClass(), BuildLocation, FRotator(0.f, BuildYaw, 0.f), P))
    {
        S->InitStructure(D->Id, false);
        FSEBuiltStructure B; B.Type = D->Id; B.Location = BuildLocation; B.Yaw = BuildYaw;
        if (USEGalaxySubsystem* G = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>()) { B.SystemIdx = G->CurrentSystem; B.PlanetIdx = G->CurrentPlanet; }
        Progression->Data.Built.Add(B);
    }
}

// ------------------------------------------------------------------ input
void ASEPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PIC)
{
    Super::SetupPlayerInputComponent(PIC);
    PIC->BindAxis("MoveForward", this, &ASEPlayerCharacter::MoveForward);
    PIC->BindAxis("MoveRight", this, &ASEPlayerCharacter::MoveRight);
    PIC->BindAxis("Turn", this, &ASEPlayerCharacter::Turn);
    PIC->BindAxis("LookUp", this, &ASEPlayerCharacter::LookUp);
    PIC->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    PIC->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    PIC->BindAction("Sprint", IE_Pressed, this, &ASEPlayerCharacter::StartSprint);
    PIC->BindAction("Sprint", IE_Released, this, &ASEPlayerCharacter::StopSprint);
    PIC->BindAction("Crouch", IE_Pressed, this, &ASEPlayerCharacter::ToggleCrouch);
    PIC->BindAction("Interact", IE_Pressed, this, &ASEPlayerCharacter::InteractPressed);
    PIC->BindAction("Scan", IE_Pressed, this, &ASEPlayerCharacter::ScanPressed);
    PIC->BindAction("BuildToggle", IE_Pressed, this, &ASEPlayerCharacter::BuildTogglePressed);
    PIC->BindAction("RotateBuild", IE_Pressed, this, &ASEPlayerCharacter::RotateBuildPressed);
    PIC->BindAction("BuildNext", IE_Pressed, this, &ASEPlayerCharacter::BuildNext);
    PIC->BindAction("BuildPrev", IE_Pressed, this, &ASEPlayerCharacter::BuildPrev);
    // The mouse button is shared with the UI click, so this binding must not consume it.
    FInputActionBinding& Press = PIC->BindAction("UseTool", IE_Pressed, this, &ASEPlayerCharacter::ToolPressed);
    Press.bConsumeInput = false;
    FInputActionBinding& Rel = PIC->BindAction("UseTool", IE_Released, this, &ASEPlayerCharacter::ToolReleased);
    Rel.bConsumeInput = false;
}

void ASEPlayerCharacter::MoveForward(float V) { if (V != 0.f && !Vitals->IsDead()) AddMovementInput(GetActorForwardVector(), V); }
void ASEPlayerCharacter::MoveRight(float V)   { if (V != 0.f && !Vitals->IsDead()) AddMovementInput(GetActorRightVector(), V); }
void ASEPlayerCharacter::Turn(float V)        { AddControllerYawInput(V * (Progression ? Progression->Settings.MouseSensitivity : 1.f)); }
void ASEPlayerCharacter::LookUp(float V)      { AddControllerPitchInput(V * (Progression ? Progression->Settings.MouseSensitivity : 1.f) * ((Progression && Progression->Settings.bInvertY) ? -1.f : 1.f)); }
void ASEPlayerCharacter::StartSprint()        { bWantsSprint = true; }
void ASEPlayerCharacter::StopSprint()         { bWantsSprint = false; }
void ASEPlayerCharacter::ToggleCrouch()       { if (bIsCrouched) UnCrouch(); else Crouch(); }

void ASEPlayerCharacter::HandleDeath()
{
    UE_LOG(LogSpaceExplorer, Log, TEXT("Player died."));
    if (Progression) Progression->PushNotice(TEXT("You died."));
    bFiring = false;
    if (APlayerController* PC = Cast<APlayerController>(GetController())) { DisableInput(PC); }
}
