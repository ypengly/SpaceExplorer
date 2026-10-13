#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SEInteractable.generated.h"

class ASEPlayerCharacter;

UINTERFACE(MinimalAPI)
class USEInteractable : public UInterface { GENERATED_BODY() };

/** Anything the player can look at and press F on: ships, NPCs, ruins, structures. */
class SPACEEXPLORER_API ISEInteractable
{
    GENERATED_BODY()
public:
    virtual void Interact(ASEPlayerCharacter* Interactor) = 0;
    virtual FText GetInteractPrompt() const { return FText::GetEmpty(); }
};

UINTERFACE(MinimalAPI)
class USEScannable : public UInterface { GENERATED_BODY() };

/** Anything the scanner (T) can discover: ore, creatures, ruins, settlements. */
class SPACEEXPLORER_API ISEScannable
{
    GENERATED_BODY()
public:
    virtual FName GetDiscoveryId() const = 0;
    virtual FString GetScanName() const = 0;
    virtual FString GetScanInfo() const { return FString(); }
    virtual int32 GetScanReward() const { return 10; }
};
