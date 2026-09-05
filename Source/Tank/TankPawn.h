#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TankPawn.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class ATankProjectile;

UCLASS()
class TANK_API ATankPawn : public APawn
{
	GENERATED_BODY()

public:
	ATankPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	virtual void PostRegisterAllComponents() override;
	virtual void BeginPlay() override;

	// ==========================================
	// Components
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UStaticMeshComponent> HullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UStaticMeshComponent> TracksMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<USceneComponent> TurretPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<USceneComponent> GunPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UStaticMeshComponent> GunMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UCameraComponent> Camera;

	// ==========================================
	// Camera Parameters (现代主流视口稳定跟随)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "5.0"))
	float CameraSensitivityX = 1.0f; // 鼠标横向旋转视口灵敏度

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera", meta = (ClampMin = "-45.0", ClampMax = "0.0", Units = "deg"))
	float FixedCameraPitch = -12.0f; // 锁定舒适第三人称俯角，彻底杜绝上下晃动眩晕

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Camera")
	float CameraRelativeYaw = 0.0f; // 视口相对车身的偏航角（鼠标横向控制，转向时自然跟随车身切入弯道）

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera")
	bool bEnableAutoCenter = false; // 直线行驶时是否自动缓动回正

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "10.0"))
	float AutoCenterSpeed = 2.0f; // 自动回正速度

	// ==========================================
	// Movement Parameters
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm/s"))
	float MoveSpeed = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg/s"))
	float TurnSpeed = 60.0f;

	// ==========================================
	// Turret & Gun Parameters (电驱伺服机械平滑追踪与双向稳定)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Turret", meta = (ClampMin = "1.0", UIMin = "5.0", UIMax = "120.0", Units = "deg/s"))
	float TurretRotateSpeed = 35.0f; // 商业标杆：主战坦克电动座圈角速度 35°/s

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Turret")
	float CurrentTurretYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "5.0"))
	float PitchSensitivity = 0.8f; // 鼠标纵向调炮灵敏度

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun")
	bool bInvertPitch = false; // 反转俯仰方向选项

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "1.0", UIMin = "5.0", UIMax = "90.0", Units = "deg/s"))
	float PitchSpeed = 20.0f; // 垂直俯仰电驱平滑角速度 20°/s，消除抽搐

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "-45.0", ClampMax = "0.0", UIMin = "-30.0", UIMax = "0.0"))
	float MinPitch = -5.0f; // 最大俯角 -5°

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "0.0", ClampMax = "85.0", UIMin = "0.0", UIMax = "45.0"))
	float MaxPitch = 20.0f; // 最大仰角 20°

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Gun")
	float DesiredGunPitch = 0.0f; // 目标火炮仰角（由鼠标Y轴直接稳定调整）

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Gun")
	float CurrentPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Aiming")
	float MaxAimDistance = 50000.0f; // 500米瞄准检测距离

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Aiming")
	bool bDrawAimDebug = true; // 启用准心落点可视化辅助

	// ==========================================
	// Combat & Firing Parameters (主炮后坐力与射击体验)
	// ==========================================
	UPROPERTY(EditDefaultsOnly, Category = "Tank|Combat")
	TSubclassOf<ATankProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat", meta = (ClampMin = "0.2", UIMin = "0.5", Units = "s"))
	float FireCooldown = 2.5f; // 2.5秒装填周期

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat", meta = (Units = "cm"))
	float MuzzleForwardOffset = 456.0f; // 炮口相对 GunPivot 顶端距离

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat", meta = (Units = "cm"))
	float RecoilDistance = -12.0f; // 开火时炮管后坐冲压距离

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat")
	float RecoilRecoverySpeed = 8.0f; // 液压驻退机复位速度

	float CurrentRecoilOffset = 0.0f;
	float LastFireTime = -100.0f;

	// ==========================================
	// Visual & Material Parameters
	// ==========================================
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TrackMaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Animation")
	FName TrackOffsetParamName = FName(TEXT("TrackOffset"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Animation", meta = (ClampMin = "1.0", UIMin = "10.0"))
	float TrackUVSpeedScale = 300.0f;

	float TrackUVOffset = 0.0f;

public:
	// Getters for external systems / UI
	FORCEINLINE float GetCurrentTurretYaw() const { return CurrentTurretYaw; }
	FORCEINLINE float GetCurrentPitch() const { return CurrentPitch; }
	FORCEINLINE float GetCurrentMoveSpeed() const { return MoveSpeed; }
	FORCEINLINE float GetReloadProgress() const 
	{ 
		const float Elapsed = GetWorld() ? (GetWorld()->GetTimeSeconds() - LastFireTime) : FireCooldown;
		return FMath::Clamp(Elapsed / FireCooldown, 0.0f, 1.0f);
	}

private:
	// Input Handlers
	void MoveForwardInput(float Value);
	void TurnInput(float Value);
	void TurnCameraInput(float Value);
	void LookUpCameraInput(float Value);
	void FireInput();

	// Manual Keyboard Overrides (保留兼容)
	void TurretRotateInput(float Value);
	void PitchUpInput(float Value);

	float CurrentMoveInput = 0.0f;
	float CurrentTurnInput = 0.0f;
	float CurrentTurretRotateInput = 0.0f;
	float CurrentPitchInput = 0.0f;
};
