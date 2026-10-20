#include "CraftingComponent.h"
#include "CraftingInventoryComponent.h"
#include "CraftingRecipe.h"
#include "CraftingStationComponent.h"
#include "CraftingSystemModule.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UCraftingComponent::UCraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCraftingComponent, Queue, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCraftingComponent, KnownRecipes, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCraftingComponent, ActiveStation, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCraftingComponent, Inventory, COND_OwnerOnly);
}

void UCraftingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!Inventory)
	{
		SetInventory(GetOwner()->FindComponentByClass<UCraftingInventoryComponent>());
	}
	if (!Inventory)
	{
		UE_LOG(LogCrafting, Warning, TEXT("%s has no UCraftingInventoryComponent. Call SetInventory before crafting."), *GetPathName());
	}

	for (UCraftingRecipe* Recipe : DefaultKnownRecipes)
	{
		UnlockRecipe(Recipe);
	}
}

void UCraftingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UnitTimerHandle);
	}
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UCraftingComponent::HandleInventoryChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UCraftingComponent::SetInventory(UCraftingInventoryComponent* NewInventory)
{
	if (!GetOwner()->HasAuthority() || NewInventory == Inventory)
	{
		return;
	}

	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UCraftingComponent::HandleInventoryChanged);
	}
	Inventory = NewInventory;
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UCraftingComponent::HandleInventoryChanged);
	}
}

void UCraftingComponent::UnlockRecipe(UCraftingRecipe* Recipe)
{
	if (!GetOwner()->HasAuthority() || !Recipe || KnownRecipes.Contains(Recipe))
	{
		return;
	}
	KnownRecipes.Add(Recipe);
	OnKnownRecipesChanged.Broadcast();
}

// --- Requests ---

ECraftingResult UCraftingComponent::RequestCraft(UCraftingRecipe* Recipe, int32 Count)
{
	const ECraftingResult Result = CanCraft(Recipe, Count);

	if (GetOwner()->HasAuthority())
	{
		if (Result == ECraftingResult::Success)
		{
			EnqueueCraft(Recipe, Count);
		}
		else
		{
			NotifyRequestRejected(Recipe, Result);
		}
	}
	else if (Result == ECraftingResult::Success)
	{
		// Only send requests the client already believes are valid; the server checks again.
		Server_RequestCraft(Recipe, Count);
	}

	return Result;
}

void UCraftingComponent::Server_RequestCraft_Implementation(UCraftingRecipe* Recipe, int32 Count)
{
	const ECraftingResult Result = CanCraft(Recipe, Count);
	if (Result == ECraftingResult::Success)
	{
		EnqueueCraft(Recipe, Count);
	}
	else
	{
		NotifyRequestRejected(Recipe, Result);
	}
}

void UCraftingComponent::CancelCraft(FGuid EntryId)
{
	if (GetOwner()->HasAuthority())
	{
		CancelCraftInternal(EntryId);
	}
	else
	{
		Server_CancelCraft(EntryId);
	}
}

void UCraftingComponent::Server_CancelCraft_Implementation(FGuid EntryId)
{
	CancelCraftInternal(EntryId);
}

void UCraftingComponent::SetActiveStation(UCraftingStationComponent* Station)
{
	if (GetOwner()->HasAuthority())
	{
		SetActiveStationInternal(Station);
	}
	else
	{
		Server_SetActiveStation(Station);
	}
}

void UCraftingComponent::Server_SetActiveStation_Implementation(UCraftingStationComponent* Station)
{
	SetActiveStationInternal(Station);
}

void UCraftingComponent::SetActiveStationInternal(UCraftingStationComponent* Station)
{
	if (Station && !Station->IsInRange(GetCrafterActor()))
	{
		UE_LOG(LogCrafting, Verbose, TEXT("%s: station %s is out of range."), *GetPathName(), *Station->GetPathName());
		return;
	}
	ActiveStation = Station;
}

// --- Queries ---

ECraftingResult UCraftingComponent::CanCraft(const UCraftingRecipe* Recipe, int32 Count) const
{
	if (!Recipe || Recipe->Outputs.IsEmpty() || Count < 1 || Count > MaxBatchSize)
	{
		return ECraftingResult::InvalidRecipe;
	}
	if (!Inventory)
	{
		return ECraftingResult::NoInventory;
	}
	if (bRequireKnownRecipes && !KnowsRecipe(Recipe))
	{
		return ECraftingResult::RecipeNotKnown;
	}
	if (Recipe->RequiresStation())
	{
		if (!ActiveStation || !ActiveStation->StationTags.HasAll(Recipe->RequiredStationTags))
		{
			return ECraftingResult::MissingStation;
		}
		if (!ActiveStation->IsInRange(GetCrafterActor()))
		{
			return ECraftingResult::StationOutOfRange;
		}
	}
	if (Queue.Num() >= MaxQueueEntries)
	{
		return ECraftingResult::QueueFull;
	}
	if (!Inventory->HasItems(Recipe->GetScaledIngredients(Count)))
	{
		return ECraftingResult::MissingIngredients;
	}
	return ECraftingResult::Success;
}

int32 UCraftingComponent::GetMaxCraftable(const UCraftingRecipe* Recipe) const
{
	if (!Recipe || !Inventory)
	{
		return 0;
	}

	int32 MaxUnits = MaxBatchSize;
	for (const FCraftingItemStack& Ingredient : Recipe->GetScaledIngredients(1))
	{
		MaxUnits = FMath::Min(MaxUnits, Inventory->GetItemCount(Ingredient.Item) / Ingredient.Quantity);
	}
	return FMath::Max(0, MaxUnits);
}

bool UCraftingComponent::KnowsRecipe(const UCraftingRecipe* Recipe) const
{
	return Recipe && KnownRecipes.Contains(Recipe);
}

TArray<UCraftingRecipe*> UCraftingComponent::GetAvailableRecipes() const
{
	TArray<UCraftingRecipe*> Result;
	for (UCraftingRecipe* Recipe : KnownRecipes)
	{
		if (!Recipe)
		{
			continue;
		}
		if (!Recipe->RequiresStation() || (ActiveStation && ActiveStation->StationTags.HasAll(Recipe->RequiredStationTags)))
		{
			Result.Add(Recipe);
		}
	}
	return Result;
}

float UCraftingComponent::GetUnitProgress(const FCraftingQueueEntry& Entry) const
{
	if (!Entry.IsStarted())
	{
		return 0.f;
	}
	if (Entry.bWaitingForSpace || Entry.UnitDuration <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(static_cast<float>((GetServerTime() - Entry.UnitStartTime) / Entry.UnitDuration), 0.f, 1.f);
}

// --- Queue processing (server) ---

void UCraftingComponent::EnqueueCraft(UCraftingRecipe* Recipe, int32 Count)
{
	if (!Inventory->RemoveItems(Recipe->GetScaledIngredients(Count)))
	{
		NotifyRequestRejected(Recipe, ECraftingResult::MissingIngredients);
		return;
	}

	const float SpeedMultiplier = ActiveStation ? FMath::Max(ActiveStation->CraftSpeedMultiplier, 0.01f) : 1.f;

	FCraftingQueueEntry& Entry = Queue.AddDefaulted_GetRef();
	Entry.Id = FGuid::NewGuid();
	Entry.Recipe = Recipe;
	Entry.RemainingCount = Count;
	Entry.UnitDuration = Recipe->CraftDuration / SpeedMultiplier;

	ProcessQueue();
}

void UCraftingComponent::CancelCraftInternal(const FGuid& EntryId)
{
	const int32 Index = Queue.IndexOfByPredicate([&EntryId](const FCraftingQueueEntry& Entry)
	{
		return Entry.Id == EntryId;
	});
	if (Index == INDEX_NONE)
	{
		return;
	}

	const FCraftingQueueEntry Cancelled = Queue[Index];
	Queue.RemoveAt(Index);
	Refund(Cancelled.Recipe, Cancelled.RemainingCount);
	ProcessQueue();
}

void UCraftingComponent::ProcessQueue()
{
	if (bIsProcessingQueue || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return;
	}
	TGuardValue<bool> ProcessingGuard(bIsProcessingQueue, true);

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	TimerManager.ClearTimer(UnitTimerHandle);

	while (Queue.Num() > 0)
	{
		FCraftingQueueEntry& Head = Queue[0];
		if (!Head.Recipe || Head.RemainingCount <= 0)
		{
			Queue.RemoveAt(0);
			continue;
		}

		if (!Head.IsStarted())
		{
			Head.UnitStartTime = GetServerTime();
		}

		const double TimeLeft = Head.UnitDuration - (GetServerTime() - Head.UnitStartTime);
		if (TimeLeft > UE_KINDA_SMALL_NUMBER)
		{
			TimerManager.SetTimer(UnitTimerHandle, this, &UCraftingComponent::ProcessQueue, static_cast<float>(TimeLeft), false);
			break;
		}

		if (!DeliverHeadUnit())
		{
			// Resumes from HandleInventoryChanged once something leaves the inventory.
			Head.bWaitingForSpace = true;
			break;
		}
	}

	BroadcastQueueChanged();
}

bool UCraftingComponent::DeliverHeadUnit()
{
	FCraftingQueueEntry& Head = Queue[0];
	UCraftingRecipe* Recipe = Head.Recipe;
	if (!Inventory || !Inventory->CanAddItems(Recipe->Outputs))
	{
		return false;
	}

	// Commit the queue before touching the inventory: inventory and completion handlers may
	// cancel entries, and a cancel must never refund a unit whose outputs were already delivered.
	if (--Head.RemainingCount <= 0)
	{
		Queue.RemoveAt(0);
	}
	else
	{
		Head.UnitStartTime = -1.0;
		Head.bWaitingForSpace = false;
	}

	Inventory->AddItems(Recipe->Outputs);
	NotifyUnitCompleted(Recipe);
	return true;
}

void UCraftingComponent::Refund(const UCraftingRecipe* Recipe, int32 Count)
{
	if (!Recipe || Count <= 0)
	{
		return;
	}

	TArray<FCraftingItemStack> Overflow;
	for (const FCraftingItemStack& Stack : Recipe->GetScaledIngredients(Count))
	{
		const int32 Added = Inventory ? Inventory->AddItem(Stack.Item, Stack.Quantity) : 0;
		if (Added < Stack.Quantity)
		{
			Overflow.Emplace(Stack.Item, Stack.Quantity - Added);
		}
	}

	if (Overflow.Num() > 0)
	{
		OnItemsOverflowed.Broadcast(Overflow);
	}
}

void UCraftingComponent::HandleInventoryChanged(UCraftingInventoryComponent* ChangedInventory)
{
	if (Queue.Num() > 0 && Queue[0].bWaitingForSpace)
	{
		ProcessQueue();
	}
}

// --- Notifications ---

void UCraftingComponent::NotifyUnitCompleted(UCraftingRecipe* Recipe)
{
	OnUnitCompleted.Broadcast(Recipe);
	if (IsOwnedByRemotePlayer())
	{
		Client_UnitCompleted(Recipe);
	}
}

void UCraftingComponent::NotifyRequestRejected(UCraftingRecipe* Recipe, ECraftingResult Reason)
{
	OnRequestRejected.Broadcast(Recipe, Reason);
	if (IsOwnedByRemotePlayer())
	{
		Client_RequestRejected(Recipe, Reason);
	}
}

void UCraftingComponent::Client_UnitCompleted_Implementation(UCraftingRecipe* Recipe)
{
	OnUnitCompleted.Broadcast(Recipe);
}

void UCraftingComponent::Client_RequestRejected_Implementation(UCraftingRecipe* Recipe, ECraftingResult Reason)
{
	OnRequestRejected.Broadcast(Recipe, Reason);
}

void UCraftingComponent::BroadcastQueueChanged()
{
	OnQueueChanged.Broadcast();
}

void UCraftingComponent::OnRep_Queue()
{
	OnQueueChanged.Broadcast();
}

void UCraftingComponent::OnRep_KnownRecipes()
{
	OnKnownRecipesChanged.Broadcast();
}

// --- Helpers ---

bool UCraftingComponent::IsOwnedByRemotePlayer() const
{
	for (const AActor* Actor = GetOwner(); Actor; Actor = Actor->GetOwner())
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(Actor))
		{
			return !PlayerController->IsLocalController();
		}
	}
	return false;
}

AActor* UCraftingComponent::GetCrafterActor() const
{
	AActor* Owner = GetOwner();
	if (const APlayerState* PlayerState = Cast<APlayerState>(Owner))
	{
		if (APawn* Pawn = PlayerState->GetPawn())
		{
			return Pawn;
		}
	}
	else if (const AController* Controller = Cast<AController>(Owner))
	{
		if (APawn* Pawn = Controller->GetPawn())
		{
			return Pawn;
		}
	}
	return Owner;
}

double UCraftingComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}
