#include "TankHealth.h"
#include "Tank.h"

UTankHealth::UTankHealth()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTankHealth::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
}

float UTankHealth::ApplyDamage(float DamageAmount)
{
	if (bDepleted || DamageAmount <= 0.0f)
	{
		return CurrentHealth;
	}
	CurrentHealth = FMath::Max(CurrentHealth - DamageAmount, 0.0f);
	OnHealthChanged.Broadcast(CurrentHealth);
	if (CurrentHealth <= 0.0f)
	{
		bDepleted = true;
		OnDepleted.Broadcast();
		UE_LOG(LogTank, Log, TEXT("[TankHealth] %s 血量耗尽"), *GetNameSafe(GetOwner()));
	}
	return CurrentHealth;
}
