#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "TankVehicle.generated.h"

class ATankProjectile;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UPhysicsAsset;
class USkeletalMeshComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UTankHealth;

/**
 * Chaos 载具坦克（docs/tank-vehicle-upgrade-plan.md 阶段 1~2）。
 *
 * 与 ATankPawn 的关系：**并存**。ATankPawn 是手写地形跟随（八点接触采样），
 * 本类是「标准载具」路线 —— 车体成为真正的 USkeletalMeshComponent 刚体，
 * 悬挂/摩擦/接地全部交给 UChaosWheeledVehicleMovementComponent（12 个物理轮）。
 * 由 ABattleGameMode::bUseChaosVehicle 决定用谁，便于 A/B 对比与回退。
 *
 * 从 ATankPawn 原样保留（与移动实现无关）：炮塔/火炮伺服、开火与后坐力、
 * 负重轮视觉、履带 UV 滚动、相机弹簧臂、血量/重生保护。
 * 被取代删除：UpdateGroundContact 全段、M1 客户端权威 ServerSyncTransform 上报、
 * 撞坡滑移、M1.5 手动挤压推进（载具互推现在由物理承担）。
 *
 * 运动学约定（与骨骼网格/物理资产严格一致）：
 *  - 骨骼网格 = 车体 + 履带，网格原点在**履带底面**；整车靠组件缩放 0.5（VehicleScale）；
 *  - 物理资产里的刚体在 root 骨上，碰撞盒 = 旧 CollisionBox 逐值同尺寸（half 380/175/118，
 *    盒心 z=120、盒底留 2cm）；
 *  - 12 根轮骨 = 负重轮轮心（网格 (X, ±144, 41.75)），物理轮有效半径 = 41.75×0.5（履带接地段）。
 */
/** 瞄准与落点探测结果（仅本地物理与 HUD 显示，零网络开销） */
USTRUCT(BlueprintType)
struct FAimTraceResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Aim")
	FVector MuzzleLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Aim")
	FVector AimPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Aim")
	FVector AimNormal = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Aim")
	float AimDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Aim")
	bool bHit = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Aim")
	bool bLockedOnEnemy = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Aim")
	TWeakObjectPtr<AActor> HitActor = nullptr;
};

UCLASS()
class TANK_API ATankVehicle : public APawn
{
	GENERATED_BODY()

public:
	ATankVehicle();

	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 重生保护期内无敌（HUD 读它显示提示）。服务器置位，复制给各端。 */
	FORCEINLINE bool IsSpawnProtected() const { return bSpawnProtected; }

	// ---- 外部 / HUD 接口 ----
	FORCEINLINE const FAimTraceResult& GetAimResult() const { return CachedAimResult; }
	FORCEINLINE float GetCurrentTurretYaw() const { return CurrentTurretYaw; }
	FORCEINLINE float GetCurrentPitch() const { return CurrentPitch; }
	FORCEINLINE USkeletalMeshComponent* GetVehicleMesh() const { return VehicleMesh; }
	FORCEINLINE UChaosWheeledVehicleMovementComponent* GetVehicleMovement() const { return VehicleMovement; }
	FORCEINLINE float GetCurrentMoveSpeed() const { return FVector::DotProduct(GetVelocity(), GetActorForwardVector()); }
	FORCEINLINE UStaticMeshComponent* GetGunMesh() const { return GunMesh; }

	FORCEINLINE float GetReloadProgress() const
	{
		const float Elapsed = GetWorld() ? (GetWorld()->GetTimeSeconds() - LastFireTime) : FireCooldown;
		return FMath::Clamp(Elapsed / FireCooldown, 0.0f, 1.0f);
	}

	/** 车体中心到顶面的高度（世界单位，已含 VehicleScale）——HUD 头顶名牌锚点 */
	float GetBodyHalfHeight() const;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitProperties() override;
	virtual void PostActorCreated() override;
	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void PostNetReceive() override;
	void AddDefaultMappingContext();

	/** 客户端两段一次性自愈，幂等，可从 BeginPlay / PostNetReceive / PossessedBy / OnRep_Controller 里重复调用：
	 *  1) Tick 注册与启用 —— 对客户端世界里的每一辆坦克都做（含远端他机：炮塔/火炮朝向在 Tick 里落地）；
	 *  2) 输入组件/动作绑定/映射上下文 —— 只对本机玩家受控 Pawn 做。 */
	void EnsureClientReady();

	/** 1s 心跳：RunUnderOneProcess 下客户端坦克的 Tick 会在运行中再次丢失，定时兜底。 */
	void RepairTickTimer();
	FTimerHandle TickRepairTimerHandle;
	bool bClientReady = false;


	/** 输入 → 左右履带驱动力矩（坦克差速：W/S 同向、A/D 反向） */
	void ApplyDriveInput();

	/** 校正骨骼网格所绑物理资产的 root 骨碰撞盒为双阶梯防托底碰撞盒 */
	static void EnsureChassisPhysicsBox(USkeletalMeshComponent* Mesh);

	/** 出生落位（仅服务器）：关卡 PlayerStart 的 Z 是按旧 Pawn「原点 = 碰撞盒心」摆的，
	 *  比载具原点（履带底面）高 60cm；悬空出生会被载具仿真的休眠逻辑冻在半空（实测），
	 *  所以这里向下扫一次把车落到地面上。 */
	void SnapToGroundOnSpawn();

	/** 视觉：负重轮转角/悬挂行程（按仿真结果）、履带 UV、炮塔/火炮/后坐力 */
	void UpdateWheelVisuals();
	void UpdateTrackScroll(float DeltaTime);
	void UpdateTurretVisuals(float DeltaTime);

	/** 屏幕调试（bDrawDebugStatus 开关）：速度/接地轮数/悬挂行程，验证期用 */
	void DrawDebugStatus();

	/** 物理链路自检（BeginPlay 与 +1.2s 各打一次）：物理状态/刚体/几何/仿真输出。
	 *  载具「不动」时这行日志能一次定位是资产没挂上、刚体没模拟，还是仿真没出数据。 */
	void LogPhysicsSetupState(const TCHAR* Phase) const;

	// ==========================================
	// Components
	// ==========================================
	/** 车体（骨骼网格 = 车体+履带，也是 Chaos 载具的刚体本体） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<USkeletalMeshComponent> VehicleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UChaosWheeledVehicleMovementComponent> VehicleMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UTankHealth> TankHealth;

	/** 12 个视觉负重轮（物理轮是骨骼上的空骨，视觉仍是静态网格） */
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
	// Vehicle Parameters（载具/整车）
	// ==========================================
	/** 整车缩放：只缩根组件（骨骼网格），视觉/骨骼位置仍用网格空间数值（与 TankPawn 一致） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Vehicle", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float VehicleScale = 0.5f;

	/** 整车质量（kg）。覆盖物理资产按体积算出来的默认质量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Vehicle", meta = (ClampMin = "100.0", Units = "Kilograms"))
	float VehicleMass = 4000.0f;

	/** 重心高度（网格空间 z，cm）。真坦克重心低，压到盒心之下更不容易在坡上翘头 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Vehicle", meta = (Units = "cm"))
	float CenterOfMassZ = 90.0f;

	/** 出生落位后的离地间隙（cm）：给悬挂留一点压缩行程，落下即接地 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Vehicle", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnGroundClearance = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Vehicle", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnSnapProbeUp = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Vehicle", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnSnapProbeDown = 2000.0f;

	// ==========================================
	// Drive Parameters（坦克差速驱动）
	// ==========================================
	/** 单轮最大驱动力矩（N·m）。12 轮合计 ≈ 12×该值/轮半径（世界 20.9cm）就是最大牵引力（1200Nm 产生 ~68982N 牵引力，充沛克服 30° 坡 19600N 下滑力） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Drive", meta = (ClampMin = "0.0"))
	float MaxDriveTorque = 1200.0f;

	/** 松手时的滑行阻力（N·m，按轮施加，方向随轮速）。400Nm 保证松手 1.7s 内平稳减速刹停，彻底消除冰上溜车 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Drive", meta = (ClampMin = "0.0"))
	float CoastBrakeTorque = 400.0f;

	/** 前进方向取反（若 W 让车倒退，勾上即可，不必改力矩正负号逻辑） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Drive")
	bool bInvertDriveDirection = false;

	/** 转向方向取反（A/D 反了时勾） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Drive")
	bool bInvertSteerDirection = false;

	/** 调试：速度/接地/悬挂行程打印 + 接地轮高亮 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Debug")
	bool bDrawDebugStatus = false;

	/** 调试/自动测试用输入覆盖：非 0 时直接当作 W/S（-1..1）与 A/D（-1..1）输入。
	 *  Python 侧可 `set_editor_property("debug_throttle", 1.0)` 驱动坦克 —— 本项目向编辑器视口
	 *  投键盘不生效（历史脚本都是驱动 PIE 客户端窗口），有它才能在单人 PIE 里做驾驶断言。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Debug", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DebugThrottle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Debug", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DebugSteer = 0.0f;

	// ==========================================
	// Camera Parameters（与 ATankPawn 同值，观感一致）
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera", meta = (ClampMin = "0.001", UIMin = "0.01", UIMax = "0.5"))
	float CameraSensitivityX = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera", meta = (ClampMin = "-45.0", ClampMax = "0.0", Units = "deg"))
	float FixedCameraPitch = -12.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Camera")
	float CameraRelativeYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float CameraArmLength = 950.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera")
	bool bEnableAutoCenter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Camera", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "10.0"))
	float AutoCenterSpeed = 2.0f;

	// ==========================================
	// Turret & Gun Parameters（照搬 ATankPawn 的伺服与手感）
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Turret", meta = (ClampMin = "1.0", UIMin = "5.0", UIMax = "200.0", Units = "deg/s"))
	float TurretRotateSpeed = 140.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Turret")
	float CurrentTurretYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "0.001", UIMin = "0.01", UIMax = "0.5"))
	float PitchSensitivity = 0.056f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun")
	bool bInvertPitch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "1.0", UIMin = "5.0", UIMax = "90.0", Units = "deg/s"))
	float PitchSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "-45.0", ClampMax = "0.0", UIMin = "-30.0", UIMax = "0.0"))
	float MinPitch = -14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Gun", meta = (ClampMin = "0.0", ClampMax = "85.0", UIMin = "0.0", UIMax = "45.0"))
	float MaxPitch = 20.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Gun")
	float DesiredGunPitch = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|Gun")
	float CurrentPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Aiming", meta = (ClampMin = "1000.0", ClampMax = "100000.0", Units = "cm"))
	float MaxAimDistance = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Aiming")
	bool bDrawAimDebug = true;

	/** 本机瞄准与落点探测缓存（每帧更新，供 HUD 准心与 3D 辅助线读写） */
	FAimTraceResult CachedAimResult;

	// ==========================================
	// Combat & Firing（同 ATankPawn）
	// ==========================================
	UPROPERTY(EditDefaultsOnly, Category = "Tank|Combat")
	TSubclassOf<ATankProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat", meta = (ClampMin = "0.2", UIMin = "0.5", Units = "s"))
	float FireCooldown = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat", meta = (Units = "cm"))
	float MuzzleForwardOffset = 456.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat", meta = (Units = "cm"))
	float RecoilDistance = -12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat")
	float RecoilRecoverySpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat", meta = (ClampMin = "0.0", UIMax = "5.0", Units = "s"))
	float SpawnProtectionDuration = 2.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SpawnProtected, BlueprintReadOnly, Category = "Tank|Combat")
	bool bSpawnProtected = false;

	// ==========================================
	// 履带 UV 滚动（与 ATankPawn 同一套参数语义）
	// ==========================================
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TrackMaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Animation")
	FName TrackOffsetParamName = FName(TEXT("TrackOffset"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Animation", meta = (ClampMin = "1.0", UIMin = "10.0"))
	float TrackUVSpeedScale = 300.0f;

	/** 履带材质在骨骼网格里的材质槽下标（0=车体 mat_61，1=履带 M_TrackScroll） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Animation", meta = (ClampMin = "0"))
	int32 TrackMaterialSlotIndex = 1;

	// ==========================================
	// Enhanced Input（动作资产复用 /Game/tank/inputs/）
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> MoveBackwardAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> TurnRightAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> TurnLeftAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> TurretCWAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> TurretCCWAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> CameraYawAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> GunPitchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Input")
	TObjectPtr<UInputAction> FireAction;

	// ==========================================
	// 炮塔/火炮朝向复制（保留项：玩家输入的本机状态，载具组件不管）
	// 阶段 3 联机改造时接回上传链路（旧路径是随 ServerSyncTransform 一起发）
	// ==========================================
	UPROPERTY(ReplicatedUsing = OnRep_NetTurretYaw, BlueprintReadOnly, Category = "Tank|Net", meta = (AllowPrivateAccess = "true"))
	float NetTurretYaw = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_NetGunPitch, BlueprintReadOnly, Category = "Tank|Net", meta = (AllowPrivateAccess = "true"))
	float NetGunPitch = 0.0f;

	UFUNCTION()
	void OnRep_NetTurretYaw();

	UFUNCTION()
	void OnRep_NetGunPitch();

	// 驱动与伺服上报频率（Hz）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Net", meta = (ClampMin = "10.0", ClampMax = "120.0", Units = "Hz"))
	float NetInputSyncFrequency = 50.0f;

private:
	// ---- Input Handlers（每帧 Triggered，Tick 末尾清零）----
	void MoveForward();
	void MoveBackward();
	void TurnRight();
	void TurnLeft();
	void TurretCW();
	void TurretCCW();
	void OrbitCamera(const FInputActionValue& Value);
	void ElevateGun(const FInputActionValue& Value);
	void Fire();

	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerUpdateDriveInput(float InThrottle, float InSteer, float InTurretYaw, float InGunPitch);

	UFUNCTION(Client, Reliable)
	void ClientSetDeathCamera(FVector_NetQuantize100 Location, FRotator Rotation);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFire(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireFX(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeathFX(FVector_NetQuantize100 DeathLoc);

	void ExecuteFire(const FVector& MuzzleLoc, const FVector& AimDir);
	void HandleDeath(AController* Killer);

	UFUNCTION()
	void OnRep_SpawnProtected();
	void ClearSpawnProtection();

	FTimerHandle SpawnProtectionTimerHandle;

	// ---- 输入状态（Triggered 每帧累加，Tick 消费后清零）----
	float CurrentThrottleInput = 0.0f;   // W=+1 / S=-1
	float CurrentSteerInput = 0.0f;      // D=+1（右转）/ A=-1
	float CurrentTurretRotateInput = 0.0f;

	// 服务端缓存的远程客户端输入（由 ServerUpdateDriveInput 刷新）
	float ReplicatedThrottleInput = 0.0f;
	float ReplicatedSteerInput = 0.0f;
	float LastDriveInputTime = -1000.0f;

	// 客户端上报限频与变动上报检测
	float LastNetInputSyncTime = -1000.0f;
	float LastSentThrottle = 0.0f;
	float LastSentSteer = 0.0f;
	float LastSentTurretYaw = 0.0f;
	float LastSentGunPitch = 0.0f;

	float CurrentRecoilOffset = 0.0f;
	float LastFireTime = -100.0f;

	float TrackUVOffset = 0.0f;

	bool bMappingContextAdded = false;

	/** 首次接地时打一条 Log（每个实例只打一次）——排查「没动」时区分输入/物理两条链 */
	bool bLoggedFirstGroundContact = false;

	/** 联调诊断：第一次收到驱动输入 / 施加油门时各打一行（每实例一次 / 限频 2/s） */
	bool bLoggedInputReceived = false;
	float LastDriveLogTime = -100.0f;
};
