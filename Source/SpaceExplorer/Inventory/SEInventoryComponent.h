#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SEInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

/** Stack-based inventory keyed by item id (FName). Item definitions arrive in Phase 8. */
UCLASS(ClassGroup=(SpaceExplorer), meta=(BlueprintSpawnableComponent))
class SPACEEXPLORER_API USEInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory") int32 MaxSlots = 40;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory") int32 MaxStackSize = 999;

    UPROPERTY(BlueprintAssignable) FOnInventoryChanged OnInventoryChanged;

    /** Returns the amount actually added (may be less than requested if full). */
    UFUNCTION(BlueprintCallable, Category="Inventory") int32 AddItem(FName ItemId, int32 Count);
    /** Returns true only if the full amount was removed. */
    UFUNCTION(BlueprintCallable, Category="Inventory") bool RemoveItem(FName ItemId, int32 Count);
    UFUNCTION(BlueprintPure, Category="Inventory") int32 GetCount(FName ItemId) const;
    UFUNCTION(BlueprintPure, Category="Inventory") bool HasItem(FName ItemId, int32 Count = 1) const { return GetCount(ItemId) >= Count; }

    const TMap<FName, int32>& GetItems() const { return Items; }
    void SetItems(const TMap<FName, int32>& InItems);

private:
    UPROPERTY() TMap<FName, int32> Items;
};
