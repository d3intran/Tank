#include "WallHealth.h"
#include "Tank.h"

UWallHealthComponent::UWallHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UWallHealthComponent::ApplyDamage(float DamageAmount)
{
	if (bDepleted || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	CurrentHealth = FMath::Max(CurrentHealth - DamageAmount, 0.0f);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		bDepleted = true;
		OnDepleted.Broadcast();
		UE_LOG(LogTank, Warning, TEXT("[WallHealth] 城墙陷落！"));
	}
	return CurrentHealth;
}
