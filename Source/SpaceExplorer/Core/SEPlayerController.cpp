#include "SEPlayerController.h"
#include "SEPlayerCharacter.h"
#include "SEShip.h"
#include "SENPC.h"
#include "SEHUD.h"
#include "SEVitalsComponent.h"
#include "SEInventoryComponent.h"
#include "SEGameData.h"
#include "SEProgression.h"
#include "SEGalaxySubsystem.h"
#include "SESaveSubsystem.h"
#include "SETerrainStreamer.h"
#include "SEPopulator.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"

void ASEPlayerController::BeginPlay()
{
    Super::BeginPlay();
    Progression = GetGameInstance()->GetSubsystem<USEProgression>();
    if (Progression && Progression->bSkipMainMenu) { Progression->bSkipMainMenu = false; bGameStarted = true; }
    else { OpenScreen(ESEScreen::MainMenu); }
    FTimerHandle H;
    GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { ApplySettings(); }), 0.6f, false);
}

void ASEPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("QuickSave", IE_Pressed, this, &ASEPlayerController::QuickSave);
    InputComponent->BindAction("QuickLoad", IE_Pressed, this, &ASEPlayerController::QuickLoad);
    // UI actions must keep working while the game is paused.
    InputComponent->BindAction("PauseToggle", IE_Pressed, this, &ASEPlayerController::PauseToggle).bExecuteWhenPaused = true;
    InputComponent->BindAction("ToggleInventory", IE_Pressed, this, &ASEPlayerController::ToggleInventory).bExecuteWhenPaused = true;
    InputComponent->BindAction("ToggleMap", IE_Pressed, this, &ASEPlayerController::ToggleMap).bExecuteWhenPaused = true;
    InputComponent->BindAction("ToggleMissions", IE_Pressed, this, &ASEPlayerController::ToggleMissions).bExecuteWhenPaused = true;
    InputComponent->BindAction("UIClick", IE_Pressed, this, &ASEPlayerController::UIClick).bExecuteWhenPaused = true;
}

void ASEPlayerController::Notice(const FString& Msg) const { if (Progression) Progression->PushNotice(Msg); }

// ---------------------------------------------------------------- screens
void ASEPlayerController::OpenScreen(ESEScreen S)
{
    if (S == ESEScreen::None) { CloseScreen(); return; }
    Screen = S; Page = 0; UIMessage.Reset();
    UGameplayStatics::SetGamePaused(this, true);
    bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}

void ASEPlayerController::CloseScreen()
{
    Screen = ESEScreen::None; TalkingNPC = nullptr;
    UGameplayStatics::SetGamePaused(this, false);
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
}

void ASEPlayerController::OpenDialogue(ASENPC* NPC)
{
    TalkingNPC = NPC; DialogueLine = -1;
    OpenScreen(ESEScreen::Dialogue);
}

void ASEPlayerController::PauseToggle()
{
    if (Screen == ESEScreen::MainMenu) return;
    if (Screen == ESEScreen::None) OpenScreen(ESEScreen::Pause); else CloseScreen();
}
void ASEPlayerController::ToggleInventory() { if (Screen == ESEScreen::MainMenu) return; if (Screen == ESEScreen::None) OpenScreen(ESEScreen::Inventory); else CloseScreen(); }
void ASEPlayerController::ToggleMap()       { if (Screen == ESEScreen::MainMenu) return; if (Screen == ESEScreen::None) OpenScreen(ESEScreen::Map); else CloseScreen(); }
void ASEPlayerController::ToggleMissions()  { if (Screen == ESEScreen::MainMenu) return; if (Screen == ESEScreen::None) OpenScreen(ESEScreen::Missions); else CloseScreen(); }

void ASEPlayerController::UIClick()
{
    float X, Y;
    if (GetMousePosition(X, Y)) { if (ASEHUD* H = Cast<ASEHUD>(GetHUD())) H->HandleClick(FVector2D(X, Y)); }
}

ASEPlayerCharacter* ASEPlayerController::GetPlayerChar() const
{
    if (ASEShip* S = Cast<ASEShip>(GetPawn())) return S->GetPilot();
    return Cast<ASEPlayerCharacter>(GetPawn());
}

ASEShip* ASEPlayerController::GetShip() const
{
    if (ASEShip* S = Cast<ASEShip>(GetPawn())) return S;
    const ASEPlayerCharacter* P = Cast<ASEPlayerCharacter>(GetPawn());
    for (TActorIterator<ASEShip> It(GetWorld()); It; ++It)
        if (P && FVector::Dist(It->GetActorLocation(), P->GetActorLocation()) < 3000.f) return *It;
    return nullptr;
}

// ---------------------------------------------------------------- game flow
void ASEPlayerController::NewGame()
{
    if (!Progression) return;
    Progression->ResetForNewGame();
    if (ASEPlayerCharacter* P = GetPlayerChar())
    {
        P->Vitals->SetVitals(P->Vitals->MaxHealth, P->Vitals->MaxOxygen, P->Vitals->MaxEnergy);
        P->Inventory->SetItems({});
        P->Inventory->AddItem(TEXT("fuel_cell"), 2);
        P->Inventory->AddItem(TEXT("oxygen_canister"), 1);
        P->Inventory->AddItem(TEXT("iron_plate"), 6);
    }
    bGameStarted = true;
    CloseScreen();
    Notice(TEXT("Welcome, explorer. Mine ore with left mouse, scan with T, board your ship with F."));
    USEGalaxySubsystem* G = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    if (G && (G->CurrentSystem != 0 || G->CurrentPlanet != 0))
    {
        if (USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>()) S->BeginTravel(0, 0);
    }
}

void ASEPlayerController::ContinueGame()
{
    USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>();
    if (!S) return;
    const TCHAR* Order[] = { TEXT("auto"), TEXT("quick"), TEXT("slot1"), TEXT("slot2"), TEXT("slot3") };
    for (const TCHAR* Slot : Order)
    {
        if (S->SlotExists(Slot) && S->LoadFromSlot(Slot)) { bGameStarted = true; CloseScreen(); return; }
    }
    UIMessage = TEXT("No save found. Start a new game.");
}

void ASEPlayerController::QuitGame(bool bSaveFirst)
{
    if (bSaveFirst) SESave(TEXT("auto"));
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ASEPlayerController::Respawn()
{
    USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>();
    bShowMouseCursor = false; SetInputMode(FInputModeGameOnly());
    if (S && S->SlotExists(TEXT("auto")) && S->LoadFromSlot(TEXT("auto"))) return;
    if (S && S->SlotExists(TEXT("quick")) && S->LoadFromSlot(TEXT("quick"))) return;
    if (ASEPlayerCharacter* P = GetPlayerChar())
    {
        P->Vitals->SetVitals(P->Vitals->MaxHealth, P->Vitals->MaxOxygen, P->Vitals->MaxEnergy);
        P->EnableInput(this);
        for (TActorIterator<ASETerrainStreamer> It(GetWorld()); It; ++It)
        {
            const FVector2D Spot = It->FindDrySpot(FVector2D::ZeroVector, 0.f);
            P->SetActorLocation(FVector(Spot.X, Spot.Y, It->GetHeightAt(Spot.X, Spot.Y) + 150.f), false, nullptr, ETeleportType::TeleportPhysics);
            break;
        }
        P->HoldForTerrain();
    }
}

void ASEPlayerController::ApplySettings()
{
    if (!Progression) return;
    if (ASEPlayerCharacter* P = GetPlayerChar()) P->ApplySettings();
    for (TActorIterator<ASETerrainStreamer> It(GetWorld()); It; ++It) { It->ViewRadius = Progression->Settings.ViewRadius; break; }
    for (TActorIterator<ASEPopulator> It(GetWorld()); It; ++It) { It->PopulateRadius = FMath::Clamp(Progression->Settings.ViewRadius / 3, 2, 4); break; }
}

// ---------------------------------------------------------------- inventory / crafting / economy
void ASEPlayerController::DoCraft(int32 Idx)
{
    USEGameData* GD = GetGameInstance()->GetSubsystem<USEGameData>();
    ASEPlayerCharacter* P = GetPlayerChar();
    if (!GD || !P || !GD->Recipes.IsValidIndex(Idx)) return;
    const FSERecipe& R = GD->Recipes[Idx];
    if (R.bNeedsFabricator && !P->IsNearFabricator()) { UIMessage = TEXT("Requires a Fabricator within 15 m."); return; }
    if (!Progression->HasTech(R.RequiredTech)) { UIMessage = TEXT("Technology not researched."); return; }
    for (const TPair<FName, int32>& In : R.Inputs)
        if (!P->Inventory->HasItem(In.Key, In.Value)) { UIMessage = TEXT("Missing ") + GD->ItemName(In.Key); return; }
    if (P->Inventory->AddItem(R.Output, R.OutputCount) <= 0) { UIMessage = TEXT("Inventory full."); return; }
    for (const TPair<FName, int32>& In : R.Inputs) P->Inventory->RemoveItem(In.Key, In.Value);
    Progression->AddXP(2);
    UIMessage = FString::Printf(TEXT("Crafted %dx %s"), R.OutputCount, *R.Name);
}

void ASEPlayerController::DoUseItem(FName Item)
{
    ASEPlayerCharacter* P = GetPlayerChar();
    if (!P || !P->Inventory->HasItem(Item)) return;
    ASEShip* Ship = Cast<ASEShip>(GetPawn());
    if (Item == TEXT("oxygen_canister")) { P->Inventory->RemoveItem(Item, 1); P->Vitals->SetVitals(P->Vitals->Health, P->Vitals->Oxygen + 60.f, P->Vitals->Energy); UIMessage = TEXT("Oxygen restored."); }
    else if (Item == TEXT("biomass")) { P->Inventory->RemoveItem(Item, 1); P->Vitals->Heal(20.f); UIMessage = TEXT("+20 health."); }
    else if (Item == TEXT("repair_kit"))
    {
        P->Inventory->RemoveItem(Item, 1);
        if (Ship) { Ship->RepairHull(50.f); UIMessage = TEXT("Ship hull repaired."); }
        else { P->Vitals->Heal(50.f); UIMessage = TEXT("+50 health."); }
    }
}

void ASEPlayerController::DoTransfer(FName Item, bool bToCargo)
{
    ASEPlayerCharacter* P = GetPlayerChar(); ASEShip* S = GetShip();
    if (!P || !S) { UIMessage = TEXT("No ship in range (30 m)."); return; }
    USEInventoryComponent* From = bToCargo ? P->Inventory.Get() : S->Cargo.Get();
    USEInventoryComponent* To = bToCargo ? S->Cargo.Get() : P->Inventory.Get();
    const int32 Added = To->AddItem(Item, From->GetCount(Item));
    From->RemoveItem(Item, Added);
    if (Added == 0) UIMessage = TEXT("No room.");
}

void ASEPlayerController::DoBuy(FName Item)
{
    USEGameData* GD = GetGameInstance()->GetSubsystem<USEGameData>();
    ASEPlayerCharacter* P = GetPlayerChar();
    if (!GD || !P || !TalkingNPC) return;
    const int32 Rep = Progression->GetRep(TalkingNPC->Faction);
    if (Rep < -30) { UIMessage = TEXT("They refuse to trade with you."); return; }
    const int32 Price = GD->BuyPrice(Item, TalkingNPC->PlanetSeed, Rep);
    if (Progression->Data.Credits < Price) { UIMessage = TEXT("Not enough credits."); return; }
    if (P->Inventory->AddItem(Item, 1) <= 0) { UIMessage = TEXT("Inventory full."); return; }
    Progression->SpendCredits(Price);
    UIMessage = FString::Printf(TEXT("Bought %s for %d."), *GD->ItemName(Item), Price);
}

void ASEPlayerController::DoSell(FName Item, bool bAll)
{
    USEGameData* GD = GetGameInstance()->GetSubsystem<USEGameData>();
    ASEPlayerCharacter* P = GetPlayerChar();
    if (!GD || !P || !TalkingNPC) return;
    const int32 Rep = Progression->GetRep(TalkingNPC->Faction);
    if (Rep < -30) { UIMessage = TEXT("They refuse to trade with you."); return; }
    const int32 N = bAll ? P->Inventory->GetCount(Item) : 1;
    if (N <= 0 || !P->Inventory->RemoveItem(Item, N)) return;
    const int32 Total = GD->SellPrice(Item, TalkingNPC->PlanetSeed, Rep) * N;
    Progression->AddCredits(Total);
    UIMessage = FString::Printf(TEXT("Sold %dx %s for %d."), N, *GD->ItemName(Item), Total);
}

void ASEPlayerController::DoAcceptMission(int32 Idx)
{
    if (!TalkingNPC || !TalkingNPC->Offers.IsValidIndex(Idx)) return;
    if (Progression->GetRep(TalkingNPC->Faction) < -30) { UIMessage = TEXT("They will not give you work."); return; }
    UIMessage = Progression->AcceptMission(TalkingNPC->Offers[Idx]) ? TEXT("Mission accepted.") : TEXT("Cannot accept (already taken, or 6 active).");
}

void ASEPlayerController::DoAbandon(int32 Idx) { Progression->AbandonMission(Idx); }

void ASEPlayerController::DoUnlockTech(FName Id)
{
    if (Progression->UnlockTech(Id))
    {
        UIMessage = TEXT("Technology unlocked.");
        if (ASEShip* S = GetShip()) S->SyncUpgradesFromTech();
    }
    else { FString Why; Progression->CanUnlock(Id, Why); UIMessage = Why; }
}

// ---------------------------------------------------------------- travel and saves
float ASEPlayerController::TravelFuelCost(int32 System, int32 Planet) const
{
    USEGalaxySubsystem* G = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    if (!G) return 0.f;
    float Cost = 10.f;
    if (System != G->CurrentSystem) Cost += FVector::Dist(G->GetSystemPosition(System), G->GetSystemPosition(G->CurrentSystem)) * 0.04f;
    return Cost;
}

void ASEPlayerController::DoTravel(int32 System, int32 Planet)
{
    USEGalaxySubsystem* G = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>();
    ASEShip* Ship = Cast<ASEShip>(GetPawn());
    if (!G || !S) return;
    if (System == G->CurrentSystem && Planet == G->CurrentPlanet) { UIMessage = TEXT("You are already here."); return; }
    if (!Ship) { UIMessage = TEXT("Board your ship and fly above the atmosphere first."); return; }
    if (!Ship->IsInSpace()) { UIMessage = TEXT("Climb above the atmosphere (hold Space) to engage the hyperdrive."); return; }
    const float Cost = TravelFuelCost(System, Planet);
    if (Ship->Fuel < Cost) { UIMessage = FString::Printf(TEXT("Needs %.0f fuel (have %.0f). Craft fuel cells from carbon."), Cost, Ship->Fuel); return; }
    Ship->Fuel -= Cost;
    CloseScreen();
    if (!S->BeginTravel(System, Planet)) { Ship->Fuel += Cost; }
}

void ASEPlayerController::DoSaveSlot(int32 Slot) { SESave(FString::Printf(TEXT("slot%d"), Slot)); UIMessage = FString::Printf(TEXT("Saved to slot %d."), Slot); }
void ASEPlayerController::DoLoadSlot(int32 Slot)
{
    USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>();
    const FString Name = FString::Printf(TEXT("slot%d"), Slot);
    if (S && S->SlotExists(Name) && S->LoadFromSlot(Name)) CloseScreen(); else UIMessage = TEXT("Slot is empty or invalid.");
}

void ASEPlayerController::ShipRefuel()
{
    ASEShip* S = GetShip(); ASEPlayerCharacter* P = GetPlayerChar();
    if (!S || !P) { UIMessage = TEXT("No ship in range."); return; }
    if (!S->Cargo->HasItem(S->FuelItemId) && P->Inventory->HasItem(S->FuelItemId)) DoTransfer(S->FuelItemId, true);
    UIMessage = S->Refuel() ? TEXT("Refuelled.") : TEXT("No fuel cells in cargo, or tank is full.");
}

void ASEPlayerController::ShipRepair()
{
    ASEShip* S = GetShip(); ASEPlayerCharacter* P = GetPlayerChar();
    if (!S || !P) { UIMessage = TEXT("No ship in range."); return; }
    USEInventoryComponent* Src = P->Inventory->HasItem(TEXT("repair_kit")) ? P->Inventory.Get() : (S->Cargo->HasItem(TEXT("repair_kit")) ? S->Cargo.Get() : nullptr);
    if (!Src || S->Hull >= S->MaxHull) { UIMessage = TEXT("No repair kit, or hull is intact."); return; }
    Src->RemoveItem(TEXT("repair_kit"), 1);
    S->RepairHull(50.f);
    UIMessage = TEXT("Hull repaired.");
}

// ---------------------------------------------------------------- console
void ASEPlayerController::SESave(const FString& Slot)
{
    USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>();
    Notice(S && S->SaveToSlot(Slot) ? FString::Printf(TEXT("Saved '%s'"), *Slot) : FString::Printf(TEXT("Save '%s' FAILED"), *Slot));
}
void ASEPlayerController::SELoad(const FString& Slot)
{
    USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>();
    Notice(S && S->LoadFromSlot(Slot) ? FString::Printf(TEXT("Loading '%s'"), *Slot) : FString::Printf(TEXT("No valid save '%s'"), *Slot));
}
void ASEPlayerController::SETravel(int32 System, int32 Planet)
{
    USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>();
    if (!S || !S->BeginTravel(System, Planet)) Notice(TEXT("Invalid destination. Use SEGalaxy and SEPlanets."));
}
void ASEPlayerController::SEPlanets()
{
    USEGalaxySubsystem* G = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    if (!G) return;
    const FSEStarSystemData Sys = G->GetCurrentSystem();
    Notice(FString::Printf(TEXT("System %d: %s (%s)"), Sys.Index, *Sys.Name, *Sys.StarClass));
    for (int32 i = 0; i < Sys.Planets.Num(); ++i)
    {
        const FSEPlanetData& P = Sys.Planets[i];
        Notice(FString::Printf(TEXT("[%d] %s %s %.0fC %.2fg %s"), i, *P.Name, *UEnum::GetValueAsString(P.Biome).RightChop(10), P.TemperatureC, P.Gravity,
            P.bBreathable ? TEXT("breathable") : (P.bHasAtmosphere ? TEXT("toxic") : TEXT("no air"))));
    }
}
void ASEPlayerController::SEGalaxy()
{
    USEGalaxySubsystem* G = GetGameInstance()->GetSubsystem<USEGalaxySubsystem>();
    if (!G) return;
    for (int32 Idx : G->GetNearestSystems(G->CurrentSystem, 6))
    {
        const FSEStarSystemData S = G->BuildSystem(Idx);
        Notice(FString::Printf(TEXT("System %d %s, %s, %d planets, %.0f ly"), Idx, *S.Name, *S.StarClass, S.Planets.Num(), FVector::Dist(S.PositionLY, G->GetSystemPosition(G->CurrentSystem))));
    }
}
void ASEPlayerController::SEGive(FName Item, int32 Count)
{
    USEInventoryComponent* Inv = nullptr;
    if (ASEShip* Ship = Cast<ASEShip>(GetPawn())) Inv = Ship->Cargo; else if (ASEPlayerCharacter* P = GetPlayerChar()) Inv = P->Inventory;
    if (Inv) Notice(FString::Printf(TEXT("Added %d x %s"), Inv->AddItem(Item, Count), *Item.ToString()));
}
void ASEPlayerController::SEResearch(int32 Points) { if (Progression) Progression->AddResearch(Points); }
void ASEPlayerController::SECredits(int32 Amount) { if (Progression) Progression->AddCredits(Amount); }
void ASEPlayerController::SETime(float Hours) { if (Progression) Progression->Data.WorldHours = FMath::Fmod(FMath::Max(0.f, Hours), 24.f); }
