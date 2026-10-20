#pragma once

#include "CoreMinimal.h"
#include "CraftingTypes.generated.h"

class UCraftingItemDefinition;
class UCraftingRecipe;

/** An item and a quantity. Used for inventory contents, recipe ingredients and recipe outputs. */
USTRUCT(BlueprintType)
struct CRAFTINGSYSTEM_API FCraftingItemStack
{
	GENERATED_BODY()

	FCraftingItemStack() = default;
	FCraftingItemStack(UCraftingItemDefinition* InItem, int32 InQuantity)
		: Item(InItem), Quantity(InQuantity)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TObjectPtr<UCraftingItemDefinition> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	bool IsValid() const { return Item != nullptr && Quantity > 0; }
};

/** Why a craft request was rejected. */
UENUM(BlueprintType)
enum class ECraftingResult : uint8
{
	Success,
	InvalidRecipe,
	RecipeNotKnown,
	MissingStation,
	StationOutOfRange,
	MissingIngredients,
	QueueFull,
	NoInventory
};

/** One entry in a crafting queue. Ingredients for the whole batch are consumed when the entry is queued. */
USTRUCT(BlueprintType)
struct CRAFTINGSYSTEM_API FCraftingQueueEntry
{
	GENERATED_BODY()

	/** Stable id so UI and cancel requests can address an entry even after the queue shifts. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	FGuid Id;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UCraftingRecipe> Recipe = nullptr;

	/** Units of this recipe still to be produced (including the one in progress). */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	int32 RemainingCount = 0;

	/** Seconds one unit takes, after the station speed multiplier was applied. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	float UnitDuration = 0.f;

	/** Server world time the current unit started, or a negative value if it has not started. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	double UnitStartTime = -1.0;

	/** True when the current unit is done but its outputs do not fit in the inventory yet. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	bool bWaitingForSpace = false;

	bool IsStarted() const { return UnitStartTime >= 0.0; }
};
