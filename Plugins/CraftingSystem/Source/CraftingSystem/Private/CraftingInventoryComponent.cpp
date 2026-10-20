#include "CraftingInventoryComponent.h"
#include "CraftingItemDefinition.h"
#include "CraftingSystemModule.h"
#include "Net/UnrealNetwork.h"

namespace CraftingInventory
{
	/** Sums quantities per item so duplicate entries in a request are checked together. */
	static TMap<UCraftingItemDefinition*, int32> MergeStacks(const TArray<FCraftingItemStack>& Stacks)
	{
		TMap<UCraftingItemDefinition*, int32> Merged;
		for (const FCraftingItemStack& Stack : Stacks)
		{
			if (Stack.IsValid())
			{
				Merged.FindOrAdd(Stack.Item.Get()) += Stack.Quantity;
			}
		}
		return Merged;
	}
}

UCraftingInventoryComponent::UCraftingInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCraftingInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCraftingInventoryComponent, Slots);
}

int32 UCraftingInventoryComponent::GetItemCount(const UCraftingItemDefinition* Item) const
{
	int32 Count = 0;
	for (const FCraftingItemStack& Slot : Slots)
	{
		if (Slot.Item == Item)
		{
			Count += Slot.Quantity;
		}
	}
	return Count;
}

bool UCraftingInventoryComponent::HasItems(const TArray<FCraftingItemStack>& Stacks) const
{
	for (const TPair<UCraftingItemDefinition*, int32>& Pair : CraftingInventory::MergeStacks(Stacks))
	{
		if (GetItemCount(Pair.Key) < Pair.Value)
		{
			return false;
		}
	}
	return true;
}

bool UCraftingInventoryComponent::CanAddItems(const TArray<FCraftingItemStack>& Stacks) const
{
	TArray<FCraftingItemStack> Simulated = Slots;
	for (const FCraftingItemStack& Stack : Stacks)
	{
		if (Stack.IsValid() && AddToSlots(Simulated, Stack.Item, Stack.Quantity, MaxSlots) > 0)
		{
			return false;
		}
	}
	return true;
}

int32 UCraftingInventoryComponent::AddItem(UCraftingItemDefinition* Item, int32 Quantity)
{
	if (!HasAuthority() || !Item || Quantity <= 0)
	{
		return 0;
	}

	const int32 Added = Quantity - AddToSlots(Slots, Item, Quantity, MaxSlots);
	if (Added > 0)
	{
		BroadcastChanged();
	}
	return Added;
}

bool UCraftingInventoryComponent::AddItems(const TArray<FCraftingItemStack>& Stacks)
{
	if (!HasAuthority() || !CanAddItems(Stacks))
	{
		return false;
	}

	for (const FCraftingItemStack& Stack : Stacks)
	{
		if (Stack.IsValid())
		{
			AddToSlots(Slots, Stack.Item, Stack.Quantity, MaxSlots);
		}
	}
	BroadcastChanged();
	return true;
}

bool UCraftingInventoryComponent::RemoveItem(UCraftingItemDefinition* Item, int32 Quantity)
{
	return RemoveItems({ FCraftingItemStack(Item, Quantity) });
}

bool UCraftingInventoryComponent::RemoveItems(const TArray<FCraftingItemStack>& Stacks)
{
	if (!HasAuthority() || !HasItems(Stacks))
	{
		return false;
	}

	for (const TPair<UCraftingItemDefinition*, int32>& Pair : CraftingInventory::MergeStacks(Stacks))
	{
		RemoveFromSlots(Slots, Pair.Key, Pair.Value);
	}
	BroadcastChanged();
	return true;
}

int32 UCraftingInventoryComponent::AddToSlots(TArray<FCraftingItemStack>& InOutSlots, UCraftingItemDefinition* Item, int32 Quantity, int32 SlotLimit)
{
	const int32 MaxStack = FMath::Max(1, Item->MaxStackSize);

	// Top up existing partial stacks first.
	for (FCraftingItemStack& Slot : InOutSlots)
	{
		if (Quantity <= 0)
		{
			break;
		}
		if (Slot.Item == Item && Slot.Quantity < MaxStack)
		{
			const int32 Moved = FMath::Min(MaxStack - Slot.Quantity, Quantity);
			Slot.Quantity += Moved;
			Quantity -= Moved;
		}
	}

	// Then open new slots while there is room.
	while (Quantity > 0 && (SlotLimit <= 0 || InOutSlots.Num() < SlotLimit))
	{
		const int32 Moved = FMath::Min(MaxStack, Quantity);
		InOutSlots.Emplace(Item, Moved);
		Quantity -= Moved;
	}

	return Quantity;
}

void UCraftingInventoryComponent::RemoveFromSlots(TArray<FCraftingItemStack>& InOutSlots, const UCraftingItemDefinition* Item, int32 Quantity)
{
	for (int32 Index = InOutSlots.Num() - 1; Index >= 0 && Quantity > 0; --Index)
	{
		FCraftingItemStack& Slot = InOutSlots[Index];
		if (Slot.Item != Item)
		{
			continue;
		}

		const int32 Taken = FMath::Min(Slot.Quantity, Quantity);
		Slot.Quantity -= Taken;
		Quantity -= Taken;
		if (Slot.Quantity <= 0)
		{
			InOutSlots.RemoveAt(Index);
		}
	}
}

bool UCraftingInventoryComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		return true;
	}
	UE_LOG(LogCrafting, Warning, TEXT("%s: inventory changes must run on the server."), *GetPathName());
	return false;
}

void UCraftingInventoryComponent::OnRep_Slots()
{
	OnInventoryChanged.Broadcast(this);
}

void UCraftingInventoryComponent::BroadcastChanged()
{
	OnInventoryChanged.Broadcast(this);
}
