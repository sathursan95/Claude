#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CraftingStationComponent.generated.h"

/**
 * Marks an actor as a crafting station (workbench, forge, campfire...).
 * Recipes whose RequiredStationTags are all present on the active station can be crafted.
 */
UCLASS(ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class CRAFTINGSYSTEM_API UCraftingStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftingStationComponent();

	/** e.g. Station.Workbench, Station.Forge.Tier2 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FGameplayTagContainer StationTags;

	/** Craft time is divided by this. 2 crafts twice as fast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (ClampMin = "0.01"))
	float CraftSpeedMultiplier = 1.f;

	/** How far the crafter's owner may be from this station when a craft is requested. Zero disables the check. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (ClampMin = "0", Units = "cm"))
	float InteractionRange = 300.f;

	UFUNCTION(BlueprintPure, Category = "Station")
	bool IsInRange(const AActor* Crafter) const;
};
