#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CraftingTypes.h"
#include "CraftingInventoryComponent.generated.h"

class UCraftingInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCraftingInventoryChanged, UCraftingInventoryComponent*, Inventory);

/**
 * Slot-based, server-authoritative item container. Slots are kept compact (no empty entries);
 * each slot holds up to the item's MaxStackSize.
 */
UCLASS(ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class CRAFTINGSYSTEM_API UCraftingInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftingInventoryComponent();

	/** Maximum number of slots. Zero means unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 MaxSlots = 30;

	/** Fires on the server after every change and on clients when the replicated slots arrive. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FCraftingInventoryChanged OnInventoryChanged;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FCraftingItemStack>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const UCraftingItemDefinition* Item) const;

	/** True if every stack's quantity is available. Duplicate items in Stacks are summed. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItems(const TArray<FCraftingItemStack>& Stacks) const;

	/** True if all stacks would fit at once. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItems(const TArray<FCraftingItemStack>& Stacks) const;

	/** Adds as many as fit and returns how many were added. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 AddItem(UCraftingItemDefinition* Item, int32 Quantity);

	/** Adds all stacks, or nothing if they do not all fit. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool AddItems(const TArray<FCraftingItemStack>& Stacks);

	/** Removes Quantity of Item, or nothing if there is not enough. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool RemoveItem(UCraftingItemDefinition* Item, int32 Quantity);

	/** Removes all stacks, or nothing if any is short. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool RemoveItems(const TArray<FCraftingItemStack>& Stacks);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Slots, Category = "Inventory")
	TArray<FCraftingItemStack> Slots;

	UFUNCTION()
	void OnRep_Slots();

private:
	/** Puts up to Quantity of Item into InOutSlots and returns the amount that did not fit. */
	static int32 AddToSlots(TArray<FCraftingItemStack>& InOutSlots, UCraftingItemDefinition* Item, int32 Quantity, int32 SlotLimit);

	/** Takes Quantity of Item out of InOutSlots, emptying the last slots first. Caller checks the count. */
	static void RemoveFromSlots(TArray<FCraftingItemStack>& InOutSlots, const UCraftingItemDefinition* Item, int32 Quantity);

	bool HasAuthority() const;
	void BroadcastChanged();
};
