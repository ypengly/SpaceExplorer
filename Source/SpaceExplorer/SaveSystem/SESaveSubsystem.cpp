#include "SESaveSubsystem.h"
#include "SESaveGame.h"
#include "SEPlayerCharacter.h"
#include "SEShip.h"
#include "SEVitalsComponent.h"
#include "SEInventoryComponent.h"
#include "SEGalaxySubsystem.h"
#include "SEProgression.h"
#include "SEWorldObjects.h"
#include "SpaceExplorer.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

ASEPlayerCharacter* USESaveSubsystem::GetPlayer() const
{
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetGameInstance(), 0);
    if (ASEShip* Ship = Cast<ASEShip>(Pawn)) { return Ship->GetPilot(); }
    return Cast<ASEPlayerCharacter>(Pawn);
}

ASEShip* USESaveSubsystem::FindShip() const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;
    for (TActorIterator<ASEShip> It(World); It; ++It) { return *It; }
    return nullptr;
}

bool USESaveSubsystem::FillSave(USESaveGame* Save) const
{
    ASEPlayerCharacter* Player = GetPlayer();
    USEGalaxySubsystem* Galaxy = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    USEProgression* Prog = GetGameInstance()->GetSubsystem<USEProgression>();
    if (!Player || !Galaxy || !Prog) return false;

    Save->SavedAt = FDateTime::UtcNow();
    Save->GalaxySeed = Galaxy->GalaxySeed;
    Save->SystemIndex = Galaxy->CurrentSystem;
    Save->PlanetIndex = Galaxy->CurrentPlanet;
    Save->PlayerLocation = Player->GetActorLocation();
    Save->PlayerRotation = Player->GetActorRotation();
    Save->Health = Player->Vitals->Health;
    Save->Oxygen = Player->Vitals->Oxygen;
    Save->Energy = Player->Vitals->Energy;
    Save->Inventory = Player->Inventory->GetItems();
    Save->Progress = Prog->Data;

    if (ASEShip* Ship = FindShip())
    {
        Save->bHasShip = true;
        Save->bPilotingShip = Ship->HasPilot();
        Save->ShipLocation = Ship->GetActorLocation();
        Save->ShipRotation = Ship->GetActorRotation();
        Save->ShipFuel = Ship->Fuel;
        Save->ShipShield = Ship->Shield;
        Save->ShipHull = Ship->Hull;
        Save->ShipEnergy = Ship->Energy;
        Save->ShipCargo = Ship->Cargo->GetItems();
    }
    return true;
}

bool USESaveSubsystem::SaveToSlot(const FString& SlotName)
{
    if (SlotName.IsEmpty()) return false;
    USESaveGame* Save = Cast<USESaveGame>(UGameplayStatics::CreateSaveGameObject(USESaveGame::StaticClass()));
    if (!FillSave(Save))
    {
        UE_LOG(LogSpaceExplorer, Warning, TEXT("SaveToSlot failed: no player or galaxy."));
        return false;
    }
    const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
    UE_LOG(LogSpaceExplorer, Log, TEXT("Save '%s': %s"), *SlotName, bOk ? TEXT("ok") : TEXT("FAILED"));
    return bOk;
}

bool USESaveSubsystem::BeginTravel(int32 System, int32 Planet)
{
    USEGalaxySubsystem* Galaxy = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    USEProgression* Prog = GetGameInstance()->GetSubsystem<USEProgression>();
    if (!Galaxy || !Prog) return false;
    USESaveGame* Snap = NewObject<USESaveGame>(this);
    if (!FillSave(Snap)) return false;
    Transit = Snap;
    Prog->bSkipMainMenu = true;
    Galaxy->PendingLoadSlot.Empty();
    return Galaxy->TravelTo(System, Planet);
}

bool USESaveSubsystem::LoadFromSlot(const FString& SlotName)
{
    USEGalaxySubsystem* Galaxy = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    USEProgression* Prog = GetGameInstance()->GetSubsystem<USEProgression>();
    if (!Galaxy || !Prog || !UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;

    USESaveGame* Save = Cast<USESaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Save) { UE_LOG(LogSpaceExplorer, Warning, TEXT("Load '%s': corrupt save."), *SlotName); return false; }
    if (Save->SaveVersion > SE_SAVE_VERSION || Save->SaveVersion < 3)
    {
        UE_LOG(LogSpaceExplorer, Warning, TEXT("Load '%s': unsupported save version %d."), *SlotName, Save->SaveVersion);
        return false;
    }
    if (Save->GalaxySeed != Galaxy->GalaxySeed || Save->SystemIndex != Galaxy->CurrentSystem || Save->PlanetIndex != Galaxy->CurrentPlanet)
    {
        Galaxy->GalaxySeed = Save->GalaxySeed;
        Galaxy->PendingLoadSlot = SlotName;
        Transit = nullptr;
        Prog->bSkipMainMenu = true;
        return Galaxy->TravelTo(Save->SystemIndex, Save->PlanetIndex);
    }
    return ApplySave(Save, true);
}

void USESaveSubsystem::CompletePendingLoad()
{
    USEGalaxySubsystem* Galaxy = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    if (!Galaxy) return;
    if (!Galaxy->PendingLoadSlot.IsEmpty())
    {
        const FString Slot = Galaxy->PendingLoadSlot;
        Galaxy->PendingLoadSlot.Empty();
        if (USESaveGame* Save = Cast<USESaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0))) { ApplySave(Save, true); }
    }
    else if (Transit)
    {
        USESaveGame* T = Transit; Transit = nullptr;
        ApplySave(T, false);
    }
}

bool USESaveSubsystem::ApplySave(USESaveGame* Save, bool bRestorePositions)
{
    ASEPlayerCharacter* Player = GetPlayer();
    USEProgression* Prog = GetGameInstance()->GetSubsystem<USEProgression>();
    if (!Player || !Prog) return false;

    if (ASEShip* Ship = FindShip()) { if (Ship->HasPilot() && bRestorePositions) Ship->Exit(true); }

    if (bRestorePositions)
    {
        Prog->Data = Save->Progress;
        ASEBaseStructure::SpawnAllBuilt(GetWorld());
        Player->SetActorLocationAndRotation(Save->PlayerLocation, Save->PlayerRotation, false, nullptr, ETeleportType::TeleportPhysics);
        if (APlayerController* PC = Cast<APlayerController>(Player->GetController())) { PC->SetControlRotation(Save->PlayerRotation); Player->EnableInput(PC); }
        Player->HoldForTerrain();
    }
    Player->Vitals->SetVitals(Save->Health, Save->Oxygen, Save->Energy);
    Player->Inventory->SetItems(Save->Inventory);

    if (Save->bHasShip)
    {
        if (ASEShip* Ship = FindShip())
        {
            if (bRestorePositions) Ship->SetActorLocationAndRotation(Save->ShipLocation, Save->ShipRotation, false, nullptr, ETeleportType::TeleportPhysics);
            Ship->SyncUpgradesFromTech();
            Ship->RestoreState(Save->ShipFuel, Save->ShipShield, Save->ShipHull, Save->ShipEnergy);
            Ship->Cargo->SetItems(Save->ShipCargo);
            if (bRestorePositions && Save->bPilotingShip) { Ship->Enter(Player); }
        }
    }
    return true;
}

bool USESaveSubsystem::SlotExists(const FString& SlotName) const { return UGameplayStatics::DoesSaveGameExist(SlotName, 0); }
bool USESaveSubsystem::DeleteSlot(const FString& SlotName) { return UGameplayStatics::DeleteGameInSlot(SlotName, 0); }
