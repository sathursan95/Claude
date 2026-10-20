#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CraftingTypes.h"
#include "CraftingComponent.generated.h"

class UCraftingInventoryComponent;
class UCraftingRecipe;
class UCraftingStationComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCraftingQueueChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCraftingKnownRecipesChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCraftingUnitCompleted, UCraftingRecipe*, Recipe);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCraftingRequestRejected, UCraftingRecipe*, Recipe, ECraftingResult, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCraftingItemsOverflowed, const TArray<FCraftingItemStack>&, Items);

/**
 * Runs a server-authoritative crafting queue against an inventory.
 *
 * Workflow: SetActiveStation (optional) -> RequestCraft -> ingredients for the whole batch are consumed
 * -> each unit takes UnitDuration -> outputs go into the inventory. If the outputs do not fit, the queue
 * waits until there is room. CancelCraft refunds the ingredients of every unit not yet delivered.
 *
 * Put it on the player's Pawn/Character, PlayerState or Controller, next to a UCraftingInventoryComponent.
 */
UCLASS(ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class CRAFTINGSYSTEM_API UCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftingComponent();

	/** Recipes known from the start. Granted on the server at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TArray<TObjectPtr<UCraftingRecipe>> DefaultKnownRecipes;

	/** When false, any recipe can be crafted without being unlocked first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	bool bRequireKnownRecipes = true;

	/** Maximum number of separate queue entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1"))
	int32 MaxQueueEntries = 5;

	/** Maximum units a single request can queue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1"))
	int32 MaxBatchSize = 99;

	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FCraftingQueueChanged OnQueueChanged;

	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FCraftingKnownRecipesChanged OnKnownRecipesChanged;

	/** Fires once per crafted unit, on the server and on the owning client. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FCraftingUnitCompleted OnUnitCompleted;

	/** Fires when the server rejects a request, on the server and on the owning client. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FCraftingRequestRejected OnRequestRejected;

	/** Server only: refunded items that did not fit in the inventory. Bind it to drop them in the world. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FCraftingItemsOverflowed OnItemsOverflowed;

	// --- Requests (call from the owning client or the server) ---

	/** Queues Count units of Recipe. Returns the local pre-check; the server re-validates and fires OnRequestRejected on failure. */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult RequestCraft(UCraftingRecipe* Recipe, int32 Count = 1);

	/** Cancels a queue entry and refunds the ingredients of its undelivered units. */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void CancelCraft(FGuid EntryId);

	/** Station whose tags and speed apply to new requests. Pass null to craft by hand. */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void SetActiveStation(UCraftingStationComponent* Station);

	// --- Queries ---

	UFUNCTION(BlueprintPure, Category = "Crafting")
	ECraftingResult CanCraft(const UCraftingRecipe* Recipe, int32 Count = 1) const;

	/** How many units the current inventory could pay for, capped at MaxBatchSize. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	int32 GetMaxCraftable(const UCraftingRecipe* Recipe) const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool KnowsRecipe(const UCraftingRecipe* Recipe) const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	const TArray<UCraftingRecipe*>& GetKnownRecipes() const { return ToRawPtrTArrayUnsafe(KnownRecipes); }

	/** Known recipes whose station requirements the active station (or hand crafting) satisfies. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<UCraftingRecipe*> GetAvailableRecipes() const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	const TArray<FCraftingQueueEntry>& GetQueue() const { return Queue; }

	/** 0..1 progress of the entry's current unit. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	float GetUnitProgress(const FCraftingQueueEntry& Entry) const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	UCraftingStationComponent* GetActiveStation() const { return ActiveStation; }

	UFUNCTION(BlueprintPure, Category = "Crafting")
	UCraftingInventoryComponent* GetInventory() const { return Inventory; }

	// --- Server only ---

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	void UnlockRecipe(UCraftingRecipe* Recipe);

	/** Use when the inventory is not on the same actor as this component. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	void SetInventory(UCraftingInventoryComponent* NewInventory);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Queue)
	TArray<FCraftingQueueEntry> Queue;

	UPROPERTY(ReplicatedUsing = OnRep_KnownRecipes)
	TArray<TObjectPtr<UCraftingRecipe>> KnownRecipes;

	UPROPERTY(Replicated)
	TObjectPtr<UCraftingStationComponent> ActiveStation;

	UPROPERTY(Replicated)
	TObjectPtr<UCraftingInventoryComponent> Inventory;

	UFUNCTION()
	void OnRep_Queue();

	UFUNCTION()
	void OnRep_KnownRecipes();

	UFUNCTION(Server, Reliable)
	void Server_RequestCraft(UCraftingRecipe* Recipe, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_CancelCraft(FGuid EntryId);

	UFUNCTION(Server, Reliable)
	void Server_SetActiveStation(UCraftingStationComponent* Station);

	UFUNCTION(Client, Reliable)
	void Client_UnitCompleted(UCraftingRecipe* Recipe);

	UFUNCTION(Client, Reliable)
	void Client_RequestRejected(UCraftingRecipe* Recipe, ECraftingResult Reason);

private:
	void EnqueueCraft(UCraftingRecipe* Recipe, int32 Count);
	void CancelCraftInternal(const FGuid& EntryId);
	void SetActiveStationInternal(UCraftingStationComponent* Station);

	/** Starts, finishes and delivers queued units until one is still running or blocked on space. */
	void ProcessQueue();

	/** Adds the head entry's outputs to the inventory. Returns false if they do not fit. */
	bool DeliverHeadUnit();

	void Refund(const UCraftingRecipe* Recipe, int32 Count);

	UFUNCTION()
	void HandleInventoryChanged(UCraftingInventoryComponent* ChangedInventory);

	void NotifyUnitCompleted(UCraftingRecipe* Recipe);
	void NotifyRequestRejected(UCraftingRecipe* Recipe, ECraftingResult Reason);
	void BroadcastQueueChanged();

	/** True when the owning player is on a remote client, so owner notifications need a Client RPC. */
	bool IsOwnedByRemotePlayer() const;

	/** The actor whose location is used for station range checks (the pawn when owned by a PlayerState or Controller). */
	AActor* GetCrafterActor() const;

	double GetServerTime() const;

	FTimerHandle UnitTimerHandle;
	bool bIsProcessingQueue = false;
};
