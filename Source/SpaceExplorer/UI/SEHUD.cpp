#include "SEHUD.h"
#include "SEPlayerController.h"
#include "SEPlayerCharacter.h"
#include "SEVitalsComponent.h"
#include "SEInventoryComponent.h"
#include "SEShip.h"
#include "SENPC.h"
#include "SEGameData.h"
#include "SEProgression.h"
#include "SEGalaxySubsystem.h"
#include "SESaveSubsystem.h"
#include "SEEnvironment.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

namespace
{
    const FLinearColor Dim(0.65f, 0.7f, 0.78f);
    const FLinearColor Gold(1.f, 0.85f, 0.3f);
    const FLinearColor Good(0.4f, 1.f, 0.5f);
    const FLinearColor Bad(1.f, 0.4f, 0.35f);

    TArray<TPair<FName, int32>> Sorted(const TMap<FName, int32>& M)
    {
        TArray<TPair<FName, int32>> Out;
        for (const TPair<FName, int32>& P : M) Out.Add(P);
        Out.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B) { return A.Key.LexicalLess(B.Key); });
        return Out;
    }
    FString Clip(const FString& S, int32 N) { return S.Len() > N ? S.Left(N - 3) + TEXT("...") : S; }
}

// ---------------------------------------------------------------- helpers
void ASEHUD::Txt(const FString& S, float X, float Y, FLinearColor C, float Scale) { DrawText(S, C, X, Y, nullptr, Scale); }

void ASEHUD::Panel(float X, float Y, float W, float H, float A) { DrawRect(FLinearColor(0.02f, 0.04f, 0.09f, A), X, Y, W, H); }

void ASEHUD::Bar(const FString& Label, float V, float Max, float X, float Y, const FLinearColor& C)
{
    const float W = 220.f, H = 14.f;
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, W, H);
    DrawRect(C, X, Y, W * FMath::Clamp(Max > 0.f ? V / Max : 0.f, 0.f, 1.f), H);
    Txt(FString::Printf(TEXT("%s %.0f"), *Label, V), X + W + 8.f, Y - 3.f, FLinearColor::White, 1.f);
}

bool ASEHUD::Btn(const FString& Label, float X, float Y, float W, float H, TFunction<void()> Fn, bool bEnabled)
{
    float MX = -1.f, MY = -1.f;
    if (PC) PC->GetMousePosition(MX, MY);
    const bool bHover = MX >= X && MX <= X + W && MY >= Y && MY <= Y + H;
    const FLinearColor Bg = !bEnabled ? FLinearColor(0.12f, 0.12f, 0.15f, 0.9f) : (bHover ? FLinearColor(0.25f, 0.5f, 0.85f, 0.95f) : FLinearColor(0.1f, 0.22f, 0.38f, 0.95f));
    DrawRect(Bg, X, Y, W, H);
    float TW, TH;
    GetTextSize(Label, TW, TH, nullptr, 1.05f);
    DrawText(Label, bEnabled ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.5f), X + (W - TW) * 0.5f, Y + (H - TH) * 0.5f, nullptr, 1.05f);
    if (bEnabled) NextButtons.Add({ FVector2D(X, Y), FVector2D(W, H), Fn });
    return bHover;
}

void ASEHUD::HandleClick(const FVector2D& P)
{
    for (int32 i = Buttons.Num() - 1; i >= 0; --i)
    {
        const FButton& B = Buttons[i];
        if (P.X >= B.Pos.X && P.X <= B.Pos.X + B.Size.X && P.Y >= B.Pos.Y && P.Y <= B.Pos.Y + B.Size.Y)
        {
            TFunction<void()> A = B.Action;
            if (A) A();
            return;
        }
    }
}

void ASEHUD::PageButtons(int32 Total, int32 PerPage, float X, float Y)
{
    const int32 Pages = FMath::Max(1, (Total + PerPage - 1) / PerPage);
    PC->Page = FMath::Clamp(PC->Page, 0, Pages - 1);
    if (Pages <= 1) return;
    Btn(TEXT("< Prev"), X, Y, 80, 28, [this]() { PC->Page = FMath::Max(0, PC->Page - 1); }, PC->Page > 0);
    Txt(FString::Printf(TEXT("Page %d/%d"), PC->Page + 1, Pages), X + 92, Y + 3, Dim, 1.f);
    Btn(TEXT("Next >"), X + 180, Y, 80, 28, [this]() { ++PC->Page; }, PC->Page < Pages - 1);
}

// ---------------------------------------------------------------- frame
void ASEHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    PC = Cast<ASEPlayerController>(GetOwningPlayerController());
    if (!PC) return;
    UGameInstance* GI = GetGameInstance();
    Prog = GI->GetSubsystem<USEProgression>(); GD = GI->GetSubsystem<USEGameData>(); Gal = GI->GetSubsystem<USEGalaxySubsystem>();
    if (!Prog || !GD || !Gal) return;
    SW = Canvas->SizeX; SH = Canvas->SizeY;
    PW = FMath::Min(1240.f, SW - 60.f); PH = FMath::Min(780.f, SH - 60.f);
    PX = (SW - PW) * 0.5f; PY = (SH - PH) * 0.5f; CX = PX + 26.f; CY = PY + 76.f;
    if (!Env.IsValid()) { for (TActorIterator<ASEEnvironment> It(GetWorld()); It; ++It) { Env = *It; break; } }

    NextButtons.Reset();
    if (PC->Screen != ESEScreen::MainMenu) DrawGameplay();
    if (PC->Screen != ESEScreen::None) DrawScreen();
    Buttons = MoveTemp(NextButtons);
}

// ---------------------------------------------------------------- in-game HUD
void ASEHUD::DrawGameplay()
{
    APawn* Pawn = GetOwningPawn();
    if (!Pawn) return;
    ASEShip* Ship = Cast<ASEShip>(Pawn);
    ASEPlayerCharacter* Player = Ship ? Ship->GetPilot() : Cast<ASEPlayerCharacter>(Pawn);

    // status bars
    float Y = SH - 150.f;
    if (Player)
    {
        const USEVitalsComponent* V = Player->Vitals;
        Bar(TEXT("HEALTH"), V->Health, V->MaxHealth, 30, Y, FLinearColor(0.85f, 0.2f, 0.2f)); Y += 22.f;
        Bar(TEXT("OXYGEN"), V->Oxygen, V->MaxOxygen, 30, Y, FLinearColor(0.3f, 0.7f, 1.f)); Y += 22.f;
        Bar(TEXT("ENERGY"), V->Energy, V->MaxEnergy, 30, Y, FLinearColor(1.f, 0.85f, 0.2f));
    }

    // planet / progression line
    const FSEPlanetData P = Gal->GetCurrentPlanet();
    const float H = Prog->Data.WorldHours;
    Txt(FString::Printf(TEXT("%s  |  %s  |  %.0fC  %.2fg  |  %s  |  %02d:%02d  %s"), *P.Name, *UEnum::GetValueAsString(P.Biome).RightChop(10), P.TemperatureC, P.Gravity,
        P.bBreathable ? TEXT("breathable") : (P.bHasAtmosphere ? TEXT("toxic air") : TEXT("no atmosphere")), (int32)H, (int32)((H - (int32)H) * 60.f), Env.IsValid() ? *Env->GetWeatherName() : TEXT("")),
        30, 20, FLinearColor(0.8f, 0.9f, 1.f), 1.1f);
    Txt(FString::Printf(TEXT("Level %d   Credits %d   Research %d      Tab: menu   M: map   J: missions   T: scan   B: build"), Prog->GetLevel(), Prog->Data.Credits, Prog->Data.ResearchPoints), 30, 44, Gold, 1.f);

    // ship panel
    if (Ship)
    {
        float SY = 80.f; const float SX = SW - 340.f;
        Bar(TEXT("FUEL"), Ship->Fuel, Ship->MaxFuel, SX, SY, FLinearColor(0.9f, 0.55f, 0.1f)); SY += 22.f;
        Bar(TEXT("SHIELD"), Ship->Shield, Ship->MaxShield, SX, SY, FLinearColor(0.3f, 0.6f, 1.f)); SY += 22.f;
        Bar(TEXT("HULL"), Ship->Hull, Ship->MaxHull, SX, SY, FLinearColor(0.8f, 0.8f, 0.8f)); SY += 22.f;
        Bar(TEXT("ENERGY"), Ship->Energy, Ship->MaxEnergy, SX, SY, FLinearColor(1.f, 0.85f, 0.2f)); SY += 26.f;
        Txt(FString::Printf(TEXT("SPEED %.0f m/s   ALT %s   %s%s%s"), Ship->GetSpeed() / 100.f,
            Ship->Altitude > 1.0e8f ? TEXT("-") : *FString::Printf(TEXT("%.0f m"), Ship->Altitude / 100.f),
            Ship->bLanded ? TEXT("LANDED ") : TEXT(""), Ship->bBoosting ? TEXT("BOOST ") : TEXT(""), Ship->IsInSpace() ? TEXT("SPACE") : TEXT("")), SX, SY, FLinearColor::White, 1.f);
        if (Ship->IsInSpace()) Txt(TEXT("Hyperdrive ready: open the map (M) to travel"), SX, SY + 22.f, Good, 1.f);
    }

    // missions tracker
    float MY = Ship ? 230.f : 80.f; const float MX = SW - 420.f;
    for (const FSEMission& M : Prog->Data.ActiveMissions)
    {
        FString Extra;
        if (M.Type == 3)
        {
            if (M.SystemIdx == Gal->CurrentSystem && M.PlanetIdx == Gal->CurrentPlanet && Player)
            {
                const FVector D = M.Location - Player->GetActorLocation();
                const float RelYaw = FMath::FindDeltaAngleDegrees(Player->GetControlRotation().Yaw, D.Rotation().Yaw);
                Extra = FString::Printf(TEXT(" - %.0f m %s"), D.Size2D() / 100.f, FMath::Abs(RelYaw) < 30.f ? TEXT("ahead") : (FMath::Abs(RelYaw) > 140.f ? TEXT("behind") : (RelYaw > 0.f ? TEXT("right") : TEXT("left"))));
            }
            else Extra = TEXT(" - on another world");
        }
        Txt(Clip(FString::Printf(TEXT("%s  [%d/%d]%s"), *M.Title, M.Progress, M.Required, *Extra), 62), MX, MY, FLinearColor::White, 1.f);
        MY += 20.f;
    }

    // notices
    const double Now = FPlatformTime::Seconds();
    float NY = SH - 230.f;
    for (int32 i = Prog->Notices.Num() - 1; i >= 0; --i)
    {
        const double Age = Now - Prog->Notices[i].Time;
        if (Age > 7.0) continue;
        Txt(Prog->Notices[i].Text, SW * 0.5f - 280.f, NY, FLinearColor(1.f, 1.f, 1.f, (float)FMath::Clamp(7.0 - Age, 0.0, 1.0)), 1.15f);
        NY -= 22.f;
    }

    if (Player)
    {
        if (Player->Vitals->Oxygen < 25.f && !Player->Vitals->IsDead()) Txt(TEXT("LOW OXYGEN"), SW * 0.5f - 60.f, 90.f, Bad, 1.5f);
        if (Player->IsExposed()) Txt(TEXT("EXTREME TEMPERATURE - research Thermal Protection or find shelter"), SW * 0.5f - 300.f, 120.f, Bad, 1.2f);
        if (Player->IsInWater()) Txt(TEXT("In water: slowed. Oxygen drains underwater."), SW * 0.5f - 200.f, 148.f, FLinearColor(0.5f, 0.8f, 1.f), 1.1f);
    }

    // crosshair, prompt, build text (on foot)
    if (!Ship && Player && PC->Screen == ESEScreen::None)
    {
        DrawRect(FLinearColor::White, SW * 0.5f - 2.f, SH * 0.5f - 2.f, 4.f, 4.f);
        const FText Prompt = Player->GetFocusPrompt();
        if (!Prompt.IsEmpty()) Txt(Prompt.ToString(), SW * 0.5f - 130.f, SH * 0.5f + 34.f, FLinearColor::Yellow, 1.2f);
        if (Player->IsBuildMode()) Txt(Player->GetBuildText(), SW * 0.5f - 300.f, SH - 120.f, Good, 1.15f);
    }

    // death
    if (Player && Player->Vitals->IsDead() && PC->Screen == ESEScreen::None)
    {
        DrawRect(FLinearColor(0.3f, 0.f, 0.f, 0.45f), 0, 0, SW, SH);
        Txt(TEXT("YOU DIED"), SW * 0.5f - 100.f, SH * 0.35f, Bad, 3.f);
        if (!bDeathCursor) { PC->bShowMouseCursor = true; FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); PC->SetInputMode(M); bDeathCursor = true; }
        Btn(TEXT("Respawn"), SW * 0.5f - 90.f, SH * 0.5f, 180, 40, [this]() { bDeathCursor = false; PC->Respawn(); });
    }
    else bDeathCursor = false;
}

// ---------------------------------------------------------------- menu frame
void ASEHUD::DrawTabs()
{
    struct FTab { const TCHAR* Name; ESEScreen S; };
    static const FTab Tabs[] = { {TEXT("Inventory"), ESEScreen::Inventory}, {TEXT("Crafting"), ESEScreen::Crafting}, {TEXT("Research"), ESEScreen::Research},
        {TEXT("Missions"), ESEScreen::Missions}, {TEXT("Map"), ESEScreen::Map}, {TEXT("Ship"), ESEScreen::Ship}, {TEXT("Codex"), ESEScreen::Encyclopedia}, {TEXT("Pause"), ESEScreen::Pause} };
    float X = PX + 20.f;
    for (const FTab& T : Tabs)
    {
        const ESEScreen S = T.S;
        if (PC->Screen == S) { DrawRect(FLinearColor(0.9f, 0.7f, 0.2f, 0.95f), X, PY + 14.f, 118.f, 34.f); Txt(T.Name, X + 12.f, PY + 21.f, FLinearColor::Black, 1.1f); }
        else Btn(T.Name, X, PY + 14.f, 118.f, 34.f, [this, S]() { PC->OpenScreen(S); });
        X += 126.f;
    }
    Btn(TEXT("X"), PX + PW - 54.f, PY + 14.f, 34.f, 34.f, [this]() { PC->CloseScreen(); });
}

void ASEHUD::DrawScreen()
{
    const ESEScreen S = PC->Screen;
    if (S == ESEScreen::MainMenu) { ScreenMainMenu(); return; }
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0, 0, SW, SH);
    Panel(PX, PY, PW, PH);
    const bool bTabbed = S != ESEScreen::Dialogue && S != ESEScreen::Trade && S != ESEScreen::Settings && S != ESEScreen::SaveLoad;
    if (bTabbed) DrawTabs();
    if (!PC->UIMessage.IsEmpty()) Txt(PC->UIMessage, PX + 26.f, PY + PH - 36.f, Gold, 1.2f);
    switch (S)
    {
    case ESEScreen::Pause: ScreenPause(); break;
    case ESEScreen::Inventory: ScreenInventory(); break;
    case ESEScreen::Crafting: ScreenCrafting(); break;
    case ESEScreen::Research: ScreenResearch(); break;
    case ESEScreen::Missions: ScreenMissions(); break;
    case ESEScreen::Map: ScreenMap(); break;
    case ESEScreen::Ship: ScreenShip(); break;
    case ESEScreen::Encyclopedia: ScreenEncyclopedia(); break;
    case ESEScreen::Dialogue: ScreenDialogue(); break;
    case ESEScreen::Trade: ScreenTrade(); break;
    case ESEScreen::Settings: ScreenSettings(); break;
    case ESEScreen::SaveLoad: ScreenSaveLoad(); break;
    default: break;
    }
}

// ---------------------------------------------------------------- screens
void ASEHUD::ScreenMainMenu()
{
    DrawRect(FLinearColor(0.01f, 0.02f, 0.05f, 1.f), 0, 0, SW, SH);
    Txt(TEXT("SPACE EXPLORER"), SW * 0.5f - 250.f, SH * 0.18f, FLinearColor(0.7f, 0.85f, 1.f), 4.f);
    Txt(TEXT("An open-world galaxy adventure"), SW * 0.5f - 130.f, SH * 0.18f + 70.f, Dim, 1.3f);
    const float X = SW * 0.5f - 120.f; float Y = SH * 0.42f;
    Btn(TEXT("New Game"), X, Y, 240, 46, [this]() { PC->NewGame(); }); Y += 58.f;
    Btn(TEXT("Continue"), X, Y, 240, 46, [this]() { PC->ContinueGame(); }); Y += 58.f;
    Btn(TEXT("Settings"), X, Y, 240, 46, [this]() { PC->OpenScreen(ESEScreen::Settings); }); Y += 58.f;
    Btn(TEXT("Quit"), X, Y, 240, 46, [this]() { PC->QuitGame(false); }); Y += 70.f;
    if (!PC->UIMessage.IsEmpty()) Txt(PC->UIMessage, X, Y, Gold, 1.2f);
}

void ASEHUD::ScreenPause()
{
    Txt(TEXT("PAUSED"), CX, CY, FLinearColor::White, 2.2f);
    const float X = CX, W = 280.f; float Y = CY + 60.f;
    Btn(TEXT("Resume"), X, Y, W, 42, [this]() { PC->CloseScreen(); }); Y += 52.f;
    Btn(TEXT("Save / Load"), X, Y, W, 42, [this]() { PC->OpenScreen(ESEScreen::SaveLoad); }); Y += 52.f;
    Btn(TEXT("Settings"), X, Y, W, 42, [this]() { PC->OpenScreen(ESEScreen::Settings); }); Y += 52.f;
    Btn(TEXT("Save and Quit"), X, Y, W, 42, [this]() { PC->QuitGame(true); });
    Txt(TEXT("Controls"), CX + 360.f, CY + 60.f, Gold, 1.3f);
    const TCHAR* Lines[] = { TEXT("WASD move, Mouse look, Space jump, Shift sprint/boost, Ctrl crouch"), TEXT("Left mouse: multitool (mine ore, fight)   T: scan   F: interact / board / exit ship"),
        TEXT("B: build mode (wheel change, R rotate, left mouse place)"), TEXT("Ship: W/S thrust, A/D strafe, Space/Ctrl up/down, Q/E roll, V camera, R refuel"),
        TEXT("Tab: inventory   M: map   J: missions   F5 quick save   F9 quick load   ~ console") };
    float LY = CY + 96.f;
    for (const TCHAR* L : Lines) { Txt(L, CX + 360.f, LY, Dim, 1.05f); LY += 24.f; }
}

void ASEHUD::ScreenSaveLoad()
{
    Txt(TEXT("SAVE / LOAD"), CX, PY + 24.f, FLinearColor::White, 1.8f);
    USESaveSubsystem* S = GetGameInstance()->GetSubsystem<USESaveSubsystem>();
    float Y = PY + 100.f;
    for (int32 i = 1; i <= 3; ++i)
    {
        const bool bExists = S && S->SlotExists(FString::Printf(TEXT("slot%d"), i));
        Txt(FString::Printf(TEXT("Slot %d   %s"), i, bExists ? TEXT("(saved)") : TEXT("(empty)")), CX, Y + 8.f, bExists ? Good : Dim, 1.3f);
        Btn(TEXT("Save"), CX + 260.f, Y, 110, 38, [this, i]() { PC->DoSaveSlot(i); });
        Btn(TEXT("Load"), CX + 380.f, Y, 110, 38, [this, i]() { PC->DoLoadSlot(i); }, bExists);
        Y += 52.f;
    }
    Txt(TEXT("Autosave and quick save (F5/F9) use their own slots."), CX, Y + 10.f, Dim, 1.1f);
    Btn(TEXT("Back"), CX, PY + PH - 80.f, 120, 38, [this]() { PC->OpenScreen(PC->bGameStarted ? ESEScreen::Pause : ESEScreen::MainMenu); });
}

void ASEHUD::ScreenSettings()
{
    Txt(TEXT("SETTINGS"), CX, PY + 24.f, FLinearColor::White, 1.8f);
    FSESettings& St = Prog->Settings;
    auto Changed = [this]() { Prog->SaveSettings(); PC->ApplySettings(); };
    float Y = PY + 100.f;
    auto Row = [&](const FString& Label, const FString& Value, TFunction<void()> Minus, TFunction<void()> Plus)
    {
        Txt(Label, CX, Y + 8.f, FLinearColor::White, 1.3f);
        Btn(TEXT("-"), CX + 300.f, Y, 44, 38, Minus);
        Txt(Value, CX + 360.f, Y + 8.f, Gold, 1.3f);
        Btn(TEXT("+"), CX + 480.f, Y, 44, 38, Plus);
        Y += 52.f;
    };
    Row(TEXT("Mouse sensitivity"), FString::Printf(TEXT("%.2f"), St.MouseSensitivity), [&St, Changed]() { St.MouseSensitivity = FMath::Max(0.2f, St.MouseSensitivity - 0.1f); Changed(); }, [&St, Changed]() { St.MouseSensitivity = FMath::Min(3.f, St.MouseSensitivity + 0.1f); Changed(); });
    Row(TEXT("Field of view"), FString::Printf(TEXT("%.0f"), St.FOV), [&St, Changed]() { St.FOV = FMath::Max(70.f, St.FOV - 5.f); Changed(); }, [&St, Changed]() { St.FOV = FMath::Min(110.f, St.FOV + 5.f); Changed(); });
    Row(TEXT("View distance (chunks)"), FString::Printf(TEXT("%d"), St.ViewRadius), [&St, Changed]() { St.ViewRadius = FMath::Max(4, St.ViewRadius - 1); Changed(); }, [&St, Changed]() { St.ViewRadius = FMath::Min(14, St.ViewRadius + 1); Changed(); });
    Row(TEXT("Invert look Y"), St.bInvertY ? TEXT("On") : TEXT("Off"), [&St, Changed]() { St.bInvertY = !St.bInvertY; Changed(); }, [&St, Changed]() { St.bInvertY = !St.bInvertY; Changed(); });
    Row(TEXT("Autosave"), St.bAutosave ? TEXT("On") : TEXT("Off"), [&St, Changed]() { St.bAutosave = !St.bAutosave; Changed(); }, [&St, Changed]() { St.bAutosave = !St.bAutosave; Changed(); });
    Btn(TEXT("Back"), CX, PY + PH - 80.f, 120, 38, [this]() { PC->OpenScreen(PC->bGameStarted ? ESEScreen::Pause : ESEScreen::MainMenu); });
}

void ASEHUD::ScreenInventory()
{
    ASEPlayerCharacter* P = PC->GetPlayerChar(); ASEShip* Ship = PC->GetShip();
    if (!P) return;
    const float RX = PX + PW * 0.5f + 10.f;
    Txt(FString::Printf(TEXT("BACKPACK  %d/%d slots"), P->Inventory->GetItems().Num(), P->Inventory->MaxSlots), CX, CY, Gold, 1.3f);
    Txt(Ship ? FString::Printf(TEXT("SHIP CARGO  %d/%d slots"), Ship->Cargo->GetItems().Num(), Ship->Cargo->MaxSlots) : FString(TEXT("SHIP CARGO  (no ship within 30 m)")), RX, CY, Gold, 1.3f);
    const int32 PerPage = 14;
    TArray<TPair<FName, int32>> L = Sorted(P->Inventory->GetItems());
    PageButtons(L.Num(), PerPage, CX, PY + PH - 80.f);
    float Y = CY + 36.f;
    for (int32 i = PC->Page * PerPage; i < FMath::Min(L.Num(), (PC->Page + 1) * PerPage); ++i)
    {
        const FName Id = L[i].Key; const FSEItemDef* D = GD->Items.Find(Id);
        Txt(FString::Printf(TEXT("%s  x%d"), *GD->ItemName(Id), L[i].Value), CX, Y + 4.f, FLinearColor::White, 1.15f);
        if (D && D->bUsable) Btn(TEXT("Use"), CX + 250.f, Y, 56, 26, [this, Id]() { PC->DoUseItem(Id); });
        if (Ship) Btn(TEXT("To ship"), CX + 312.f, Y, 80, 26, [this, Id]() { PC->DoTransfer(Id, true); });
        Y += 32.f;
    }
    if (L.Num() == 0) Txt(TEXT("Empty. Mine ore with the multitool (left mouse)."), CX, Y, Dim, 1.1f);
    if (Ship)
    {
        float RY = CY + 36.f;
        for (const TPair<FName, int32>& E : Sorted(Ship->Cargo->GetItems()))
        {
            const FName Id = E.Key;
            Txt(FString::Printf(TEXT("%s  x%d"), *GD->ItemName(Id), E.Value), RX, RY + 4.f, FLinearColor::White, 1.15f);
            Btn(TEXT("To pack"), RX + 250.f, RY, 80, 26, [this, Id]() { PC->DoTransfer(Id, false); });
            RY += 32.f;
        }
    }
}

void ASEHUD::ScreenCrafting()
{
    ASEPlayerCharacter* P = PC->GetPlayerChar();
    if (!P) return;
    Txt(TEXT("CRAFTING"), CX, CY, Gold, 1.4f);
    Txt(P->IsNearFabricator() ? TEXT("Fabricator in range: advanced recipes available") : TEXT("Build a Fabricator (needs Advanced Habitats research) for advanced recipes"), CX + 220.f, CY + 4.f, P->IsNearFabricator() ? Good : Dim, 1.05f);
    float Y = CY + 44.f;
    for (int32 i = 0; i < GD->Recipes.Num(); ++i)
    {
        const FSERecipe& R = GD->Recipes[i];
        bool bCan = Prog->HasTech(R.RequiredTech) && (!R.bNeedsFabricator || P->IsNearFabricator());
        FString Ing;
        for (const TPair<FName, int32>& In : R.Inputs)
        {
            const int32 Have = P->Inventory->GetCount(In.Key);
            if (Have < In.Value) bCan = false;
            Ing += FString::Printf(TEXT("%dx %s (%d)   "), In.Value, *GD->ItemName(In.Key), Have);
        }
        Txt(FString::Printf(TEXT("%s%s"), *R.Name, R.bNeedsFabricator ? TEXT("  [Fabricator]") : TEXT("")), CX, Y, bCan ? FLinearColor::White : Dim, 1.2f);
        Txt(Ing, CX + 24.f, Y + 22.f, Dim, 1.0f);
        Btn(TEXT("Craft"), PX + PW - 160.f, Y + 4.f, 110, 34, [this, i]() { PC->DoCraft(i); }, bCan);
        Y += 62.f;
    }
}

void ASEHUD::ScreenResearch()
{
    Txt(FString::Printf(TEXT("RESEARCH   points: %d   (earn by scanning, exploring ruins and completing missions)"), Prog->Data.ResearchPoints), CX, CY, Gold, 1.3f);
    float Y = CY + 44.f;
    for (const FSETechNode& T : GD->Tech)
    {
        const bool bHas = Prog->HasTech(T.Id);
        FString Why; const bool bCan = Prog->CanUnlock(T.Id, Why);
        Txt(FString::Printf(TEXT("%s   (%d)"), *T.Name, T.Cost), CX, Y, bHas ? Good : FLinearColor::White, 1.15f);
        Txt(bHas ? T.Desc : (bCan ? T.Desc : T.Desc + TEXT("   - ") + Why), CX + 24.f, Y + 21.f, Dim, 1.0f);
        if (bHas) Txt(TEXT("Unlocked"), PX + PW - 150.f, Y + 6.f, Good, 1.1f);
        else { const FName Id = T.Id; Btn(TEXT("Unlock"), PX + PW - 160.f, Y + 2.f, 110, 32, [this, Id]() { PC->DoUnlockTech(Id); }, bCan); }
        Y += 50.f;
    }
}

void ASEHUD::ScreenMissions()
{
    Txt(TEXT("ACTIVE MISSIONS"), CX, CY, Gold, 1.4f);
    float Y = CY + 40.f;
    if (Prog->Data.ActiveMissions.Num() == 0) Txt(TEXT("None. Talk to settlers (F) to take work."), CX, Y, Dim, 1.1f);
    for (int32 i = 0; i < Prog->Data.ActiveMissions.Num(); ++i)
    {
        const FSEMission& M = Prog->Data.ActiveMissions[i];
        Txt(FString::Printf(TEXT("%s   [%d/%d]   from %s"), *M.Title, M.Progress, M.Required, *M.GiverName), CX, Y, FLinearColor::White, 1.2f);
        Txt(FString::Printf(TEXT("%s   Reward: %d credits, %d research, %d rep"), *M.Desc, M.RewardCredits, M.RewardResearch, M.RewardRep), CX + 20.f, Y + 22.f, Dim, 1.0f);
        Btn(TEXT("Abandon"), PX + PW - 150.f, Y + 4.f, 110, 30, [this, i]() { PC->DoAbandon(i); });
        Y += 62.f;
    }
    Y += 10.f;
    Txt(FString::Printf(TEXT("Completed: %d"), Prog->Data.CompletedMissions.Num()), CX, Y, Good, 1.2f);
    Y += 34.f;
    Txt(TEXT("REPUTATION"), CX, Y, Gold, 1.3f); Y += 28.f;
    for (const FSEFaction& F : GD->Factions) { Txt(FString::Printf(TEXT("%s: %d"), *F.Name, Prog->GetRep(F.Id)), CX, Y, F.Color, 1.15f); Y += 24.f; }
}

void ASEHUD::ScreenMap()
{
    if (MapForSystem != Gal->CurrentSystem) { MapList = Gal->GetNearestSystems(Gal->CurrentSystem, 40); MapForSystem = Gal->CurrentSystem; }
    if (PC->SelectedSystem < 0) PC->SelectedSystem = Gal->CurrentSystem;
    const FVector Origin = Gal->GetSystemPosition(Gal->CurrentSystem);
    float MaxD = 1.f;
    for (int32 Idx : MapList) MaxD = FMath::Max(MaxD, FVector::Dist2D(Gal->GetSystemPosition(Idx), Origin));
    const float MW = 560.f, MH = PH - 130.f;
    const FVector2D C(CX + MW * 0.5f, CY + MH * 0.5f + 10.f);
    const float Scale = (FMath::Min(MW, MH) * 0.5f - 24.f) / MaxD;
    Txt(TEXT("GALAXY MAP (nearest 40 systems)"), CX, CY - 4.f, Gold, 1.3f);
    DrawRect(FLinearColor(0.f, 0.f, 0.03f, 0.9f), CX, CY + 24.f, MW, MH);
    auto Dot = [&](int32 Idx)
    {
        const FVector D = Gal->GetSystemPosition(Idx) - Origin;
        const FVector2D P = C + FVector2D(D.X, D.Y) * Scale;
        const bool bSel = Idx == PC->SelectedSystem, bCur = Idx == Gal->CurrentSystem;
        if (bSel) DrawRect(FLinearColor(1.f, 0.85f, 0.2f), P.X - 9.f, P.Y - 9.f, 18.f, 18.f);
        Btn(TEXT(""), P.X - 6.f, P.Y - 6.f, 12.f, 12.f, [this, Idx]() { PC->SelectedSystem = Idx; });
        if (bCur) Txt(TEXT("YOU"), P.X + 10.f, P.Y - 8.f, Good, 0.9f);
    };
    Dot(Gal->CurrentSystem);
    for (int32 Idx : MapList) Dot(Idx);

    const float RX = CX + MW + 24.f;
    const FSEStarSystemData Sys = Gal->BuildSystem(PC->SelectedSystem);
    const float Ly = FVector::Dist(Sys.PositionLY, Origin);
    Txt(FString::Printf(TEXT("%s  (#%d)"), *Sys.Name, Sys.Index), RX, CY, Sys.StarColor, 1.5f);
    Txt(FString::Printf(TEXT("%s   %.0f ly away   %d asteroid fields"), *Sys.StarClass, Ly, Sys.AsteroidFieldCount), RX, CY + 28.f, Dim, 1.05f);
    Txt(TEXT("Hyperdrive: pilot your ship above the atmosphere, then pick a world."), RX, CY + 50.f, Dim, 1.0f);
    float Y = CY + 84.f;
    const int32 PerPage = 11;
    PageButtons(Sys.Planets.Num(), PerPage, RX, PY + PH - 80.f);
    for (int32 i = PC->Page * PerPage; i < FMath::Min(Sys.Planets.Num(), (PC->Page + 1) * PerPage); ++i)
    {
        const FSEPlanetData& P = Sys.Planets[i];
        const bool bHere = Sys.Index == Gal->CurrentSystem && i == Gal->CurrentPlanet;
        const bool bCharted = Prog->Data.Discoveries.Contains(FName(*FString::Printf(TEXT("planet:%d:%d"), Sys.Index, i)));
        Txt(FString::Printf(TEXT("[%d] %s%s"), i, *P.Name, P.bIsMoon ? TEXT(" (moon)") : TEXT("")), RX, Y, bHere ? Good : FLinearColor::White, 1.1f);
        Txt(bCharted ? FString::Printf(TEXT("%s  %.0fC  %.2fg  %s"), *UEnum::GetValueAsString(P.Biome).RightChop(10), P.TemperatureC, P.Gravity, P.bBreathable ? TEXT("breathable") : (P.bHasAtmosphere ? TEXT("toxic") : TEXT("no air")))
                       : FString(TEXT("Uncharted: visit to reveal details")), RX + 14.f, Y + 19.f, Dim, 0.95f);
        const int32 Sys_ = Sys.Index, Pl = i;
        Btn(bHere ? TEXT("Here") : *FString::Printf(TEXT("Go (%.0f fuel)"), PC->TravelFuelCost(Sys.Index, i)), RX + 380.f, Y + 2.f, 150.f, 30.f, [this, Sys_, Pl]() { PC->DoTravel(Sys_, Pl); }, !bHere);
        Y += 46.f;
    }
}

void ASEHUD::ScreenShip()
{
    ASEShip* S = PC->GetShip();
    Txt(TEXT("SHIP"), CX, CY, Gold, 1.4f);
    if (!S) { Txt(TEXT("No ship within 30 m."), CX, CY + 40.f, Dim, 1.2f); return; }
    float Y = CY + 44.f;
    auto Line = [&](const FString& T, FLinearColor C = FLinearColor::White) { Txt(T, CX, Y, C, 1.2f); Y += 26.f; };
    Line(FString::Printf(TEXT("Hull %.0f/%.0f     Shield %.0f/%.0f     Energy %.0f/%.0f"), S->Hull, S->MaxHull, S->Shield, S->MaxShield, S->Energy, S->MaxEnergy));
    Line(FString::Printf(TEXT("Fuel %.0f/%.0f     Top speed %.0f m/s (atmosphere)     Cargo slots %d"), S->Fuel, S->MaxFuel, S->MaxSpeed / 100.f, S->Cargo->MaxSlots));
    Y += 10.f;
    Line(TEXT("UPGRADES (unlocked through Research)"), Gold);
    Line(FString::Printf(TEXT("Engine level %d    Fuel tank level %d    Shield level %d    Cargo level %d"), Prog->TechLevel(TEXT("engine_")), Prog->TechLevel(TEXT("fuel_")), Prog->TechLevel(TEXT("shield_")), Prog->TechLevel(TEXT("cargo_"))), Dim);
    Y += 16.f;
    Btn(TEXT("Refuel (fuel cell)"), CX, Y, 220, 40, [this]() { PC->ShipRefuel(); });
    Btn(TEXT("Repair hull (repair kit)"), CX + 240.f, Y, 260, 40, [this]() { PC->ShipRepair(); });
    Y += 60.f;
    Line(TEXT("Craft fuel cells from carbon and iron plates; craft repair kits at a Fabricator."), Dim);
}

void ASEHUD::ScreenEncyclopedia()
{
    TArray<TPair<FString, FString>> L;
    for (const TPair<FName, FString>& D : Prog->Data.Discoveries) L.Add({ D.Key.ToString(), D.Value });
    L.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B) { return A.Key < B.Key; });
    Txt(FString::Printf(TEXT("CODEX   %d discoveries (creatures, resources, structures, settlements, worlds)"), L.Num()), CX, CY, Gold, 1.3f);
    const int32 PerPage = 18;
    PageButtons(L.Num(), PerPage, CX, PY + PH - 80.f);
    float Y = CY + 36.f;
    for (int32 i = PC->Page * PerPage; i < FMath::Min(L.Num(), (PC->Page + 1) * PerPage); ++i)
    {
        FString Kind = L[i].Key; int32 Colon; if (Kind.FindChar(':', Colon)) Kind = Kind.Left(Colon);
        Txt(FString::Printf(TEXT("[%s]  %s"), *Kind, *Clip(L[i].Value, 120)), CX, Y, FLinearColor::White, 1.05f);
        Y += 24.f;
    }
    if (L.Num() == 0) Txt(TEXT("Nothing yet. Aim at something and press T."), CX, Y, Dim, 1.1f);
}

void ASEHUD::ScreenDialogue()
{
    ASENPC* N = PC->TalkingNPC;
    if (!N) { PC->CloseScreen(); return; }
    const FSEFaction* F = GD->FindFaction(N->Faction);
    Txt(FString::Printf(TEXT("%s - %s"), *N->NPCName, *N->Job), CX, PY + 24.f, F ? F->Color : FLinearColor::White, 1.8f);
    Txt(FString::Printf(TEXT("%s   |   reputation %d   |   currently %s"), F ? *F->Name : TEXT(""), Prog->GetRep(N->Faction), *N->GetActivity().ToLower()), CX, PY + 62.f, Dim, 1.05f);
    Txt(PC->DialogueLine < 0 ? N->GetGreeting() : N->GetTalkLines()[PC->DialogueLine % N->GetTalkLines().Num()], CX, PY + 110.f, FLinearColor::White, 1.4f);
    Btn(TEXT("Talk"), CX, PY + 170.f, 140, 38, [this]() { ++PC->DialogueLine; });
    Btn(TEXT("Trade"), CX + 150.f, PY + 170.f, 140, 38, [this]() { PC->OpenScreen(ESEScreen::Trade); });
    Btn(TEXT("Leave"), CX + 300.f, PY + 170.f, 140, 38, [this]() { PC->CloseScreen(); });
    Txt(TEXT("WORK AVAILABLE"), CX, PY + 240.f, Gold, 1.3f);
    float Y = PY + 274.f;
    for (int32 i = 0; i < N->Offers.Num(); ++i)
    {
        const FSEMission& M = N->Offers[i];
        const bool bTaken = Prog->IsMissionTaken(M.Id);
        Txt(M.Title, CX, Y, bTaken ? Dim : FLinearColor::White, 1.2f);
        Txt(FString::Printf(TEXT("%s   Reward: %d credits, %d research, %d rep"), *Clip(M.Desc, 80), M.RewardCredits, M.RewardResearch, M.RewardRep), CX + 14.f, Y + 22.f, Dim, 1.0f);
        Btn(bTaken ? TEXT("Taken") : TEXT("Accept"), PX + PW - 160.f, Y + 4.f, 110, 32, [this, i]() { PC->DoAcceptMission(i); }, !bTaken);
        Y += 66.f;
    }
}

void ASEHUD::ScreenTrade()
{
    ASENPC* N = PC->TalkingNPC; ASEPlayerCharacter* P = PC->GetPlayerChar();
    if (!N || !P) { PC->CloseScreen(); return; }
    const int32 Rep = Prog->GetRep(N->Faction);
    Txt(FString::Printf(TEXT("TRADE with %s     Credits: %d"), *N->NPCName, Prog->Data.Credits), CX, PY + 24.f, Gold, 1.6f);
    const float RX = PX + PW * 0.5f + 10.f;
    Txt(TEXT("FOR SALE"), CX, PY + 80.f, FLinearColor::White, 1.3f);
    Txt(TEXT("YOUR GOODS"), RX, PY + 80.f, FLinearColor::White, 1.3f);
    float Y = PY + 116.f;
    for (FName Id : N->Stock)
    {
        const int32 Price = GD->BuyPrice(Id, N->PlanetSeed, Rep);
        Txt(FString::Printf(TEXT("%s   %d cr"), *GD->ItemName(Id), Price), CX, Y + 4.f, FLinearColor::White, 1.15f);
        Btn(TEXT("Buy"), CX + 330.f, Y, 80, 28, [this, Id]() { PC->DoBuy(Id); }, Prog->Data.Credits >= Price);
        Y += 34.f;
    }
    Y = PY + 116.f;
    for (const TPair<FName, int32>& E : Sorted(P->Inventory->GetItems()))
    {
        const FName Id = E.Key;
        Txt(FString::Printf(TEXT("%s x%d   %d cr"), *GD->ItemName(Id), E.Value, GD->SellPrice(Id, N->PlanetSeed, Rep)), RX, Y + 4.f, FLinearColor::White, 1.15f);
        Btn(TEXT("Sell"), RX + 300.f, Y, 70, 28, [this, Id]() { PC->DoSell(Id, false); });
        Btn(TEXT("All"), RX + 378.f, Y, 60, 28, [this, Id]() { PC->DoSell(Id, true); });
        Y += 34.f;
    }
    Btn(TEXT("Back"), CX, PY + PH - 80.f, 120, 38, [this]() { PC->OpenScreen(ESEScreen::Dialogue); });
}
