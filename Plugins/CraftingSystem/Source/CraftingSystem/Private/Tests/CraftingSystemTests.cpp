#include "CraftingInventoryComponent.h"
#include "CraftingItemDefinition.h"
#include "CraftingRecipe.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CraftingTests
{
	/** Standalone game world with one actor that owns an inventory (standalone actors have authority). */
	struct FTestInventory
	{
		UWorld* World = nullptr;
		UCraftingInventoryComponent* Inventory = nullptr;

		explicit FTestInventory(int32 MaxSlots)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

			AActor* Owner = World->SpawnActor<AActor>();
			Inventory = NewObject<UCraftingInventoryComponent>(Owner);
			Inventory->MaxSlots = MaxSlots;
			Inventory->RegisterComponent();
		}

		~FTestInventory()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	static UCraftingItemDefinition* MakeItem(int32 MaxStackSize)
	{
		UCraftingItemDefinition* Item = NewObject<UCraftingItemDefinition>(GetTransientPackage());
		Item->MaxStackSize = MaxStackSize;
		return Item;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCraftingInventoryStackingTest, "CraftingSystem.Inventory.Stacking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingInventoryStackingTest::RunTest(const FString& Parameters)
{
	CraftingTests::FTestInventory Test(2);
	UCraftingItemDefinition* Wood = CraftingTests::MakeItem(10);

	TestEqual(TEXT("Fits in two stacks"), Test.Inventory->AddItem(Wood, 15), 15);
	TestEqual(TEXT("Uses two slots"), Test.Inventory->GetSlots().Num(), 2);
	TestEqual(TEXT("Only 5 more fit"), Test.Inventory->AddItem(Wood, 8), 5);
	TestEqual(TEXT("Count is capped"), Test.Inventory->GetItemCount(Wood), 20);
	TestFalse(TEXT("Full inventory rejects more"), Test.Inventory->CanAddItems({ FCraftingItemStack(Wood, 1) }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCraftingInventoryAtomicTest, "CraftingSystem.Inventory.AtomicChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingInventoryAtomicTest::RunTest(const FString& Parameters)
{
	CraftingTests::FTestInventory Test(1);
	UCraftingItemDefinition* Wood = CraftingTests::MakeItem(10);
	UCraftingItemDefinition* Stone = CraftingTests::MakeItem(10);

	Test.Inventory->AddItem(Wood, 5);

	TestFalse(TEXT("AddItems rejects a batch that does not fit"),
		Test.Inventory->AddItems({ FCraftingItemStack(Wood, 2), FCraftingItemStack(Stone, 1) }));
	TestEqual(TEXT("Rejected AddItems changes nothing"), Test.Inventory->GetItemCount(Wood), 5);

	TestFalse(TEXT("RemoveItems sums duplicate entries"),
		Test.Inventory->RemoveItems({ FCraftingItemStack(Wood, 3), FCraftingItemStack(Wood, 3) }));
	TestEqual(TEXT("Rejected RemoveItems changes nothing"), Test.Inventory->GetItemCount(Wood), 5);

	TestTrue(TEXT("Exact removal succeeds"), Test.Inventory->RemoveItem(Wood, 5));
	TestEqual(TEXT("Empty stacks are removed"), Test.Inventory->GetSlots().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCraftingRecipeScalingTest, "CraftingSystem.Recipe.ScaledIngredients",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingRecipeScalingTest::RunTest(const FString& Parameters)
{
	UCraftingItemDefinition* Wood = CraftingTests::MakeItem(99);
	UCraftingItemDefinition* Nail = CraftingTests::MakeItem(99);

	UCraftingRecipe* Recipe = NewObject<UCraftingRecipe>(GetTransientPackage());
	Recipe->Ingredients = { FCraftingItemStack(Wood, 2), FCraftingItemStack(Nail, 4), FCraftingItemStack(Wood, 1) };

	const TArray<FCraftingItemStack> Scaled = Recipe->GetScaledIngredients(3);
	TestEqual(TEXT("Duplicate ingredients are merged"), Scaled.Num(), 2);
	TestEqual(TEXT("Wood is scaled"), Scaled[0].Quantity, 9);
	TestEqual(TEXT("Nails are scaled"), Scaled[1].Quantity, 12);
	TestEqual(TEXT("Zero count needs nothing"), Recipe->GetScaledIngredients(0).Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
