#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CraftingTypes.h"
#include "CraftingRecipe.generated.h"

/**
 * Turns a set of ingredient stacks into a set of output stacks over time.
 * Create one data asset per recipe (Data Asset > CraftingRecipe).
 */
UCLASS(BlueprintType, Const)
class CRAFTINGSYSTEM_API UCraftingRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FText DisplayName;

	/** Consumed from the crafter's inventory, per unit crafted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TArray<FCraftingItemStack> Ingredients;

	/** Added to the crafter's inventory, per unit crafted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TArray<FCraftingItemStack> Outputs;

	/** Seconds to craft one unit at a station with a speed multiplier of 1. Zero crafts instantly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "0", Units = "s"))
	float CraftDuration = 1.f;

	/** The active station must have all of these tags. Leave empty for recipes craftable anywhere (hand crafting). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FGameplayTagContainer RequiredStationTags;

	/** Category tags for UI filtering, e.g. Recipe.Category.Tools. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FGameplayTagContainer RecipeTags;

	/** Ingredients for Count units, with duplicate items merged. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<FCraftingItemStack> GetScaledIngredients(int32 Count) const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool RequiresStation() const { return !RequiredStationTags.IsEmpty(); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
