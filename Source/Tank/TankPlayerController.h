#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TankPlayerController.generated.h"

/**
 * 坦克对战的 PlayerController。
 *
 * 目前只解决一件事：**阵亡期间镜头会掉到地图原点朝天**。
 *
 * 原因在引擎里（PlayerController.cpp:994）：
 *     void APlayerController::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
 *     {
 *         OutResult.Location = GetFocalLocation();   // 无 Pawn 时退化成 PC 自身位置
 *         OutResult.Rotation = GetControlRotation(); // PC 的操控朝向
 *     }
 * Pawn 被销毁后 PC 既没被移动过、操控朝向也不是坦克的朝向，于是镜头跑到原点。
 * 而 AController::SetActorLocation 是 private，外部没法直接挪 PC——
 * 所以在这里覆盖 CalcCamera 真正读的那两个函数，阵亡期间返回"最后的观战位姿"。
 *
 * 顺带的好处：这两个覆盖只在「无 Pawn 且有记录」时生效，一旦新坦克占有就自动走回引擎默认，
 * 不需要任何清理逻辑。
 */
UCLASS()
class TANK_API ATankPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATankPlayerController();

	/** 由 TankPawn 在阵亡时告知：把镜头留在残骸处。 */
	void SetDeathViewLocation(const FVector& InLocation, const FRotator& InRotation);

	//~ 阵亡期间用最后已知位姿代替「PC 自身位置 / 操控朝向」
	virtual FVector GetFocalLocation() const override;
	virtual FRotator GetControlRotation() const override;

protected:
	/** 重新占有（重生）时清掉阵亡观战位姿，交还给引擎默认行为。 */
	virtual void OnPossess(APawn* InPawn) override;

private:
	FVector DeathViewLocation = FVector::ZeroVector;
	FRotator DeathViewRotation = FRotator::ZeroRotator;
	bool bHasDeathView = false;
};
