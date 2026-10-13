#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SEPlayerController.generated.h"

class ASENPC;
class ASEPlayerCharacter;
class ASEShip;
class USEProgression;

UENUM()
enum class ESEScreen : uint8
{
    None, MainMenu, Pause, Inventory, Crafting, Research, Missions, Map, Ship, Encyclopedia, Dialogue, Trade, Settings, SaveLoad
};

/** Owns UI state and every action a menu button can trigger, plus developer console commands. */
UCLASS()
class SPACEEXPLORER_API ASEPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    // ---- UI state (read by the HUD)
    ESEScreen Screen = ESEScreen::None;
    int32 Page = 0;
    int32 SelectedSystem = -1;
    FString UIMessage;
    UPROPERTY() TObjectPtr<ASENPC> TalkingNPC;
    int32 DialogueLine = 0;
    /** False until the player picks New Game / Continue; decides where "Back" in Settings returns to. */
    bool bGameStarted = false;

    void OpenScreen(ESEScreen S);
    void CloseScreen();
    void OpenDialogue(ASENPC* NPC);
    bool IsUIOpen() const { return Screen != ESEScreen::None; }

    ASEPlayerCharacter* GetPlayerChar() const;
    ASEShip* GetShip() const;

    // ---- actions
    void NewGame();
    void ContinueGame();
    void QuitGame(bool bSaveFirst);
    void Respawn();
    void DoCraft(int32 RecipeIndex);
    void DoUseItem(FName ItemId);
    void DoTransfer(FName ItemId, bool bToCargo);
    void DoBuy(FName ItemId);
    void DoSell(FName ItemId, bool bAll);
    void DoAcceptMission(int32 OfferIndex);
    void DoAbandon(int32 ActiveIndex);
    void DoUnlockTech(FName TechId);
    void DoTravel(int32 System, int32 Planet);
    void DoSaveSlot(int32 Slot);
    void DoLoadSlot(int32 Slot);
    void ShipRepair();
    void ShipRefuel();
    void ApplySettings();
    float TravelFuelCost(int32 System, int32 Planet) const;

    // ---- console
    UFUNCTION(Exec) void SESave(const FString& Slot);
    UFUNCTION(Exec) void SELoad(const FString& Slot);
    UFUNCTION(Exec) void SETravel(int32 System, int32 Planet);
    UFUNCTION(Exec) void SEPlanets();
    UFUNCTION(Exec) void SEGalaxy();
    UFUNCTION(Exec) void SEGive(FName ItemId, int32 Count);
    UFUNCTION(Exec) void SEResearch(int32 Points);
    UFUNCTION(Exec) void SECredits(int32 Amount);
    UFUNCTION(Exec) void SETime(float Hours);

    virtual void BeginPlay() override;
protected:
    virtual void SetupInputComponent() override;

private:
    void PauseToggle();
    void ToggleInventory();
    void ToggleMap();
    void ToggleMissions();
    void UIClick();
    void QuickSave() { SESave(TEXT("quick")); }
    void QuickLoad() { SELoad(TEXT("quick")); }
    void Notice(const FString& Msg) const;
    UPROPERTY() TObjectPtr<USEProgression> Progression;
};
