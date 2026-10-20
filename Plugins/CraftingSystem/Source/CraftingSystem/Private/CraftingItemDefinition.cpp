#include "CraftingItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "CraftingItemDefinition"

const FPrimaryAssetType UCraftingItemDefinition::AssetType(TEXT("CraftingItem"));

FPrimaryAssetId UCraftingItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UCraftingItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (DisplayName.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoDisplayName", "Item has no DisplayName."));
	}

	if (MaxStackSize < 1)
	{
		Context.AddError(LOCTEXT("BadStackSize", "MaxStackSize must be at least 1."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
