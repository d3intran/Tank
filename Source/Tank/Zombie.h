#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Zombie.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UClimbManager;
class ATankPawn;

UENUM(BlueprintType)
enum class EZombieState : uint8
{
	WalkToWall UMETA(DisplayName = "冲向大门"),
	AtPile     UMETA(DisplayName = "占位挤堆"),
	Climbing   UMETA(DisplayName = "沿墙攀爬"),
	Trampled   UMETA(DisplayName = "被踩倒"),
	Dead       UMETA(DisplayName = "死亡")
};

/**
 * P2a 灰盒丧尸：槽位尸堆模型（方案 A：登顶为唯一伤害源）。
 * 行为链：出生 → 扫掠冲向大门 → 进堆区领槽位 lerp 过去 →
 * 槽满后沿墙垂直上升 → 到墙顶消失扣血。任意阶段被击杀切击飞坠落态。
 */
UCLASS(Blueprintable)
class TANK_API AZombie : public AActor
{
	GENERATED_BODY()

public:
	AZombie();

	virtual void Tick(float DeltaTime) override;

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	void SetTargetWall(AActor* InTargetWall) { TargetWall = InTargetWall; }

	UFUNCTION(BlueprintPure, Category = "Zombie|State")
	FORCEINLINE EZombieState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Zombie|State")
	FORCEINLINE bool IsDead() const { return State == EZombieState::Dead; }

	UFUNCTION(BlueprintPure, Category = "Zombie|State")
	FORCEINLINE float GetHealth() const { return Health; }

protected:
	virtual void BeginPlay() override;

private:
	void Die(bool bCrushed, const FVector& FlingDir);
	void VanishAtTop();
	void LieDown();
	void UpdateFacing(const FVector& MoveDir, float DeltaTime);
	void GroundStepTo(const FVector& Target, float DeltaTime, float SpeedScale);
	bool IsCrushedByTank() const;
	FVector ComputeSeparationOffset(float DeltaTime) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Separation", meta = (AllowPrivateAccess = "true", ClampMin = "40.0", UIMin = "60.0", Units = "cm"))
	float SeparationRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Separation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "20.0", Units = "cm/s"))
	float SeparationStrength = 80.0f;

	// ==========================================
	// Components
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	// ==========================================
	// Parameters
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "10.0", UIMin = "50.0", Units = "cm/s"))
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "60.0", UIMin = "120.0", Units = "deg/s"))
	float TurnSpeed = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Climb", meta = (AllowPrivateAccess = "true", ClampMin = "50.0", UIMin = "100.0", Units = "cm/s"))
	float SlotLerpSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Climb", meta = (AllowPrivateAccess = "true", ClampMin = "30.0", UIMin = "80.0", Units = "cm/s"))
	float ClimbSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "10.0"))
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", ClampMax = "1.0", UIMin = "0.3"))
	float WoundedSpeedScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", UIMin = "300.0", Units = "cm"))
	float CrushRadius = 450.0f;

	// ==========================================
	// Runtime
	// ==========================================
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Zombie|State", meta = (AllowPrivateAccess = "true"))
	EZombieState State = EZombieState::WalkToWall;

	TWeakObjectPtr<AActor> TargetWall;
	TWeakObjectPtr<UClimbManager> CachedClimbManager;
	TWeakObjectPtr<ATankPawn> CachedTank;

	FVector PileCenter = FVector::ZeroVector;
	float BaseOriginZ = 0.0f;

	int32 AssignedSlot = INDEX_NONE;
	FVector SlotTarget = FVector::ZeroVector;

	// 死亡击飞（坠落态：弹道 → 落地回缩）
	FVector DeadVelocity = FVector::ZeroVector;
	float GroundZ = 0.0f;
	bool bCorpseSettled = false;
	bool bWounded = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterialInstance;
};
