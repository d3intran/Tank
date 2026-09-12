#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "TankPawn.generated.h"

class UBoxComponent;
class UTankHealth;
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
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 重生保护期内无敌（HUD 也读这个显示提示）。服务器权威，复制给各端。 */
	FORCEINLINE bool IsSpawnProtected() const { return bSpawnProtected; }
protected:
	virtual void PostRegisterAllComponents() override;
	virtual void BeginPlay() override;
	/** 客户端每收到一次复制更新（含首次）都会走这里，是客户端做一次性初始化的正确钩子
	 *  （`BeginPlay` 在客户端可能早于复制状态到达；`PostNetInit` 不是常规复制路径的初始化钩子）。
	 *  这里挂客户端自愈：客户端世界的坦克 Tick 注册/输入绑定会整个丢掉（PIE 三开实测），
	 *  表现为「只能开炮不能移动」+「看不到别人炮塔转向」——两者都靠 Tick 落地。
	 *  幂等由 EnsureClientReady 内部闸门保证。 */
	virtual void PostNetReceive() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	void AddDefaultMappingContext();

	/** 客户端两段一次性自愈，幂等，可从 BeginPlay / PostNetReceive / PossessedBy / OnRep_Controller
	 *  里重复调用：
	 *  1) Tick 注册与启用 —— 对客户端世界里的**每一辆**坦克都做（含远端他机：炮塔/火炮朝向、
	 *     后坐力复位、从位移反推负重轮都在 Tick 里落地）；
	 *  2) 输入组件/动作绑定/映射上下文 —— 只对「本机玩家的本机 Pawn」做。 */
	void EnsureClientReady();

	// ==========================================
	// Components
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components")
	TObjectPtr<UTankHealth> TankHealth;

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
	// M3 重生保护（FFA 混战防落地秒杀）
	// ==========================================
	/** 出生后无敌时长（秒）。0 = 关闭保护 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Combat", meta = (ClampMin = "0.0", UIMax = "5.0", Units = "s"))
	float SpawnProtectionDuration = 2.0f;

	/** 当前是否处于保护期。服务器置位，复制给各端供 HUD 显示 */
	UPROPERTY(ReplicatedUsing = OnRep_SpawnProtected, BlueprintReadOnly, Category = "Tank|Combat")
	bool bSpawnProtected = false;

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

	// M1 客户端权威移动同步：本机客户端 50Hz 上报位姿，服务器应用后经移动复制转发其他端。
	// Unreliable：位置流，丢一包下一包就补上，不需要可靠重传
	//
	// TurretYaw/GunPitch 顺带一起上报：炮塔与火炮朝向同样是「本机权威、服务器不知道」的状态
	// （开火时才随 ServerFire 上报一次），不上报的话敌方视角只能看到车体旋转、
	// 炮塔永远朝着车头方向。
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerSyncTransform(const FVector_NetQuantize100& Location, const FRotator& NetRotation,
		float TurretYaw, float GunPitch);

	// M2 服务端权威开火：客户端只报炮口位姿（炮塔俯仰是本机状态，服务器不知道），
	// 炮弹生成/模拟/命中判定全部服务器独占；表现（后坐力/弹道闪）走 Multicast
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFire(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireFX(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeathFX(FVector_NetQuantize100 DeathLoc);

	/**
	 * 阵亡后把镜头留在残骸处（只在被击杀玩家自己的客户端执行）。
	 *
	 * 为什么需要：Pawn 被销毁后 PC 失去 ViewTarget，引擎的
	 * `APlayerController::CalcCamera` 退化成
	 *     OutResult.Location = GetFocalLocation();   // 无 Pawn 时 = PC 自身位置
	 *     OutResult.Rotation = GetControlRotation(); // PC 的操控朝向
	 * 而 PC 从来没被移动过，镜头就会掉到地图原点/朝天（灰盒观感很糟，演示时很扎眼）。
	 *
	 * 注意 AController::SetActorLocation 是 private，外部挪不动 PC；
	 * 因此实际由 TankPlayerController 覆盖 GetFocalLocation / GetControlRotation 来接管，
	 * 本 RPC 只负责把「阵亡位姿」送过去。新坦克占有后自动交还引擎默认，无需清理。
	 */
	UFUNCTION(Client, Reliable)
	void ClientSetDeathCamera(FVector_NetQuantize100 Location, FRotator Rotation);

	void ExecuteFire(const FVector& MuzzleLoc, const FVector& AimDir);
	void HandleDeath(AController* Killer);

	UFUNCTION()
	void OnRep_SpawnProtected();

	/** 保护期结束回调（仅服务器） */
	void ClearSpawnProtection();

	FTimerHandle SpawnProtectionTimerHandle;

	// M1.5 挤压推进：本机扫掠被对方坦克挡住时，把对方沿推进方向顶开。
	// 路由：服务器判被推端归属——主机自有直接应用，客户端自有走 Client RPC 让其所有者本地应用
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerPushTank(ATankPawn* PushedTank, FVector_NetQuantize10 PushDelta);

	UFUNCTION(Client, Unreliable)
	void ClientApplyPush(FVector_NetQuantize10 PushDelta);

	void TryPushTank(ATankPawn* HitTank, const FVector& PushDelta);
	void ExecuteTankPush(ATankPawn* PushedTank, const FVector& PushDelta);
	void ApplyPushDelta(const FVector& PushDelta);

	// 挤压推力倍率：1=顶车即以本机推进速度推走对方，0=关闭挤压
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Net", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "2.0"))
	float PushStrength = 1.0f;

	// 被推位移消化速度（cm/s）：推挤 RPC 按推动方帧率到达，节奏差会造成跳帧；
	// 到达的推挤量累积进 PendingPushOffset，按此恒定速度平滑消化
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Net", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", ClampMax = "3000.0", Units = "cm/s"))
	float PushConsumeSpeed = 1200.0f;

	// 上报间隔（秒）。20ms=50Hz：PIE 单进程下接近每帧同步；真插值缓冲留 M4
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Net", meta = (AllowPrivateAccess = "true", ClampMin = "0.02", ClampMax = "0.5"))
	float TransformSyncInterval = 0.02f;
	float LastTransformSyncTime = -1000.0f;

	// ==========================================
	// 炮塔/火炮朝向的远端表现（本机状态 → 服务器 → 其他端）
	// ==========================================
	// 炮塔转向与火炮俯仰只由本机输入驱动（TankPawn::Tick 里的伺服），服务器完全不知道，
	// 于是敌方视角只能看到车体旋转、炮塔永远朝车头。做法沿用 M1 的位姿上报链路：
	//   本机 50Hz 随 ServerSyncTransform 上报 → 服务器写入下面两个复制属性 → 转发给其他端
	// COND_SkipOwner：拥有者本地自己算，不需要再吃一次回传，省一半带宽。
	// 远端实例在 Tick 里直接采用这两个值，不跑伺服（跑了会被 CameraRelativeYaw=0 拽回车头）。
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Tank|Net", meta = (AllowPrivateAccess = "true"))
	float NetTurretYaw = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Tank|Net", meta = (AllowPrivateAccess = "true"))
	float NetGunPitch = 0.0f;

	bool bMappingContextAdded = false;

	// Tick 自愈的一次性闸门。与 bClientReady 分开：Tick 修复对客户端世界里的每一辆坦克都要做
	// （含远端他机——它们的炮塔朝向也靠 Tick 落地），输入修复只对本机自己的车做
	bool bTickRepaired = false;

	// 输入自愈的一次性闸门（必须在拿到本机 Controller 之后才能做）
	bool bClientReady = false;

	// 本机位移首次真正生效时打一条 Log（每个实例只打一次）——排查「客户端只能开炮不能移动」时
	// 这一行能直接区分「输入没到」和「Tick 没跑」，不需要再开 Verbose 刷屏
	bool bLoggedFirstLocalMove = false;

	// 待消化的被推位移（本地累积，Tick 恒速消耗；不复制——被推端本地应用后经常规位姿上报收敛）
	FVector PendingPushOffset = FVector::ZeroVector;

	float CurrentMoveInput = 0.0f;
	float CurrentTurnInput = 0.0f;
	float CurrentTurretRotateInput = 0.0f;

	// 负重轮累计转角（度），与 RoadWheels 一一对应
	TArray<float> RoadWheelAngles;

	/** 按左右履带线速度推进负重轮转角（cm/s）。本机走输入、远端走位移反推，共用这段 */
	void UpdateWheelSpin(float DeltaTime, float TrackSpeedLeft, float TrackSpeedRight);

	// 远端负重轮反推用的上一帧位姿。
	// 本机受控实例的输入在服务器/其他客户端上恒为 0，若只靠输入驱动轮子，
	// 远端坦克就会"车在滑行、轮子不动"——所以非本机实例要从实际位移反推履带线速度
	FVector LastTickLocation = FVector::ZeroVector;
	float LastTickYaw = 0.0f;
	bool bHasLastTickPose = false;
};
