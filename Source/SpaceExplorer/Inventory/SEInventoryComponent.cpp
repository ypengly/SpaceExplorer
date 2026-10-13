#include "SEInventoryComponent.h"

int32 USEInventoryComponent::AddItem(FName ItemId, int32 Count)
{
    if (ItemId.IsNone() || Count <= 0) return 0;
    const bool bNewSlot = !Items.Contains(ItemId);
    if (bNewSlot && Items.Num() >= MaxSlots) return 0;

    int32& Stack = Items.FindOrAdd(ItemId);
    const int32 Added = FMath::Max(0, FMath::Min(Count, MaxStackSize - Stack));
    Stack += Added;
    if (Stack <= 0) Items.Remove(ItemId);
    if (Added > 0) OnInventoryChanged.Broadcast();
    return Added;
}

bool USEInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
    int32* Stack = Items.Find(ItemId);
    if (!Stack || Count <= 0 || *Stack < Count) return false;
    *Stack -= Count;
    if (*Stack == 0) Items.Remove(ItemId);
    OnInventoryChanged.Broadcast();
    return true;
}

int32 USEInventoryComponent::GetCount(FName ItemId) const
{
    const int32* Stack = Items.Find(ItemId);
    return Stack ? *Stack : 0;
}

void USEInventoryComponent::SetItems(const TMap<FName, int32>& InItems)
{
    Items = InItems;
    OnInventoryChanged.Broadcast();
}
