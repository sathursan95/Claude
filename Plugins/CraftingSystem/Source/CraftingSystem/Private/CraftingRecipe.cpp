#include "CraftingRecipe.h"
#include "CraftingItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "CraftingRecipe"

const FPrimaryAssetType UCraftingRecipe::AssetType(TEXT("CraftingRecipe"));

TArray<FCraftingItemStack> UCraftingRecipe::GetScaledIngredients(int32 Count) const
{
	TArray<FCraftingItemStack> Result;
	if (Count <= 0)
	{
		return Result;
	}

	for (const FCraftingItemStack& Ingredient : Ingredients)
	{
		if (!Ingredient.IsValid())
		{
			continue;
		}

		FCraftingItemStack* Existing = Result.FindByPredicate([&Ingredient](const FCraftingItemStack& Stack)
		{
			return Stack.Item == Ingredient.Item;
		});

		if (Existing)
		{
			Existing->Quantity += Ingredient.Quantity * Count;
		}
		else
		{
			Result.Emplace(Ingredient.Item, Ingredient.Quantity * Count);
		}
	}
	return Result;
}

FPrimaryAssetId UCraftingRecipe::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UCraftingRecipe::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Outputs.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoOutputs", "Recipe has no Outputs."));
		Result = EDataValidationResult::Invalid;
	}

	auto ValidateStacks = [&Context, &Result](const TArray<FCraftingItemStack>& Stacks, const FText& Label)
	{
		for (int32 Index = 0; Index < Stacks.Num(); ++Index)
		{
			if (!Stacks[Index].IsValid())
			{
				Context.AddError(FText::Format(LOCTEXT("BadStack", "{0}[{1}] needs an Item and a Quantity of at least 1."), Label, Index));
				Result = EDataValidationResult::Invalid;
			}
		}
	};
	ValidateStacks(Ingredients, LOCTEXT("Ingredients", "Ingredients"));
	ValidateStacks(Outputs, LOCTEXT("Outputs", "Outputs"));

	if (CraftDuration < 0.f)
	{
		Context.AddError(LOCTEXT("NegativeDuration", "CraftDuration cannot be negative."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
