#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SEHUD.generated.h"

class ASEPlayerController;
class ASEEnvironment;
class USEProgression;
class USEGameData;
class USEGalaxySubsystem;

/** Canvas HUD and every menu screen. Each button calls into ASEPlayerController, which owns the game logic. */
UCLASS()
class SPACEEXPLORER_API ASEHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    void HandleClick(const FVector2D& Pos);

private:
    struct FButton { FVector2D Pos; FVector2D Size; TFunction<void()> Action; };
    TArray<FButton> Buttons, NextButtons;

    ASEPlayerController* PC = nullptr;
    USEProgression* Prog = nullptr;
    USEGameData* GD = nullptr;
    USEGalaxySubsystem* Gal = nullptr;
    TWeakObjectPtr<ASEEnvironment> Env;
    float SW = 0.f, SH = 0.f, PX = 0.f, PY = 0.f, PW = 0.f, PH = 0.f, CX = 0.f, CY = 0.f;
    TArray<int32> MapList;
    int32 MapForSystem = -1;
    bool bDeathCursor = false;

    void Txt(const FString& S, float X, float Y, FLinearColor C = FLinearColor::White, float Scale = 1.2f);
    void Bar(const FString& Label, float Value, float Max, float X, float Y, const FLinearColor& Color);
    void Panel(float X, float Y, float W, float H, float Alpha = 0.9f);
    bool Btn(const FString& Label, float X, float Y, float W, float H, TFunction<void()> Fn, bool bEnabled = true);
    void PageButtons(int32 Total, int32 PerPage, float X, float Y);

    void DrawGameplay();
    void DrawScreen();
    void DrawTabs();
    void ScreenMainMenu(); void ScreenPause(); void ScreenInventory(); void ScreenCrafting(); void ScreenResearch();
    void ScreenMissions(); void ScreenMap(); void ScreenShip(); void ScreenEncyclopedia(); void ScreenDialogue();
    void ScreenTrade(); void ScreenSettings(); void ScreenSaveLoad();
};
