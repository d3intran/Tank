#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TankHealth.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTankHealthChanged, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTankDepleted);

/**
 * 坦克血量组件（M0 灰盒）：纯逻辑组件挂 TankPawn，炮弹命中经 ApplyPointDamage 走 Actor 的 TakeDamage 后调这里。
 * M2 联机升级：MaxHealth/CurrentHealth 改 Replicated + OnRep_Health。
 */
UCLASS(ClassGroup = (Battle), meta = (BlueprintSpawnableComponent))
class TANK_API UTankHealth : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankHealth();

	// 扣血入口。返回扣后剩余值，已耗尽时幂等返回 0
	float ApplyDamage(float DamageAmount);

	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	FORCEINLINE bool IsDepleted() const { return bDepleted; }

	UPROPERTY(BlueprintAssignable, Category = "Tank|Health")
	FOnTankHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Tank|Health")
	FOnTankDepleted OnDepleted;

protected:
	virtual void BeginPlay() override;

	// 血量 1000：炮弹直击 250 → 4 发击毁（灰盒基准，M4 平衡时调）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Health", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "100.0"))
	float MaxHealth = 1000.0f;

	float CurrentHealth = 0.0f;
	bool bDepleted = false;
};
