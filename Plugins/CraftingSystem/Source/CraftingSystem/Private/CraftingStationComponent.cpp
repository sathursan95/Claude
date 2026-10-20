#include "CraftingStationComponent.h"
#include "GameFramework/Actor.h"

UCraftingStationComponent::UCraftingStationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UCraftingStationComponent::IsInRange(const AActor* Crafter) const
{
	const AActor* Station = GetOwner();
	if (!Station || !Crafter)
	{
		return false;
	}
	if (InteractionRange <= 0.f)
	{
		return true;
	}
	return FVector::DistSquared(Station->GetActorLocation(), Crafter->GetActorLocation()) <= FMath::Square(InteractionRange);
}
