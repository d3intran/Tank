#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "TankPawn.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class ATankProjectile;
class UInputAction;
class UInputMappingContext;

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
	TArray<TObjectPtr<UStaticMeshComponent>> RoadWheels;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera", meta = (ClampMin = "0.001", UIMin = "0.01", UIMax = "0.5"))
	float CameraSensitivityX = 0.07f; // 鼠标横向旋转视口灵敏度（Enhanced 原始鼠标量纲）

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
	float MoveSpeed = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg/s"))
	float TurnSpeed = 60.0f;

	// ==========================================
	// Wheel Parameters (负重轮差速旋转，布局由 Scripts/split_tank_mesh.py 生成)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Wheels", meta = (ClampMin = "1.0", Units = "cm"))
	float TrackSpan = 278.4f; // 左右侧履带间距，转向差速计算用

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Wheels")
	bool bInvertWheelSpin = false; // 视觉上轮子转向与行驶方向相反时勾选

	// ==========================================
	// Turret & Gun Parameters (电驱伺服机械平滑追踪与双向稳定)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Turret", meta = (ClampMin = "1.0", UIMin = "5.0", UIMax = "120.0", Units = "deg/s"))
	float TurretRotateSpeed = 70.0f; // 炮塔回转角速度（2× 灵敏度，基准 35°/s）

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Turret")
	float CurrentTurretYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "0.001", UIMin = "0.01", UIMax = "0.5"))
	float PitchSensitivity = 0.056f; // 鼠标纵向调炮灵敏度（Enhanced 原始鼠标量纲）

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

	// ==========================================
	// Enhanced Input（动作资产在 /Game/tank/inputs/，由 MCP 创建并保存）
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> MoveForwardAction;   // W：前进

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> MoveBackwardAction;  // S：倒退

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> TurnRightAction;     // D：右转

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> TurnLeftAction;      // A：左转

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> TurretCWAction;      // E：炮塔顺时针微调

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> TurretCCWAction;     // Q：炮塔逆时针微调

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> CameraYawAction;     // 鼠标 X：视口偏航

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> GunPitchAction;      // 鼠标 Y：火炮俯仰

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> FireAction;          // 左键：主炮开火

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
	// Input Handlers（Enhanced Input 回调，方向由独立动作资产表达）
	void MoveForward();
	void MoveBackward();
	void TurnRight();
	void TurnLeft();
	void TurretCW();
	void TurretCCW();
	void OrbitCamera(const FInputActionValue& Value);
	void ElevateGun(const FInputActionValue& Value);
	void Fire();

	float CurrentMoveInput = 0.0f;
	float CurrentTurnInput = 0.0f;
	float CurrentTurretRotateInput = 0.0f;
	float CurrentPitchInput = 0.0f;

	// 负重轮累计转角（度），与 RoadWheels 一一对应
	TArray<float> RoadWheelAngles;
};
