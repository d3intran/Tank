#include "TankPawn.h"
#include "Tank.h"
#include "TankProjectile.h"
#include "TankHealth.h"
#include "BattleGameMode.h"
#include "TankPlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	// 负重轮滚动半径（cm），均匀 z_span 69.8/2；Y 取 144 让轮盘端面退到履带
	// 外侧面（Y≈171）之后 ~3cm，避免旋转时与履带侧面 z-fighting 闪烁
	constexpr float RoadWheelRollRadius = 35.0f;
	constexpr float RoadWheelY = 144.0f;
	constexpr float RoadWheelZ = 41.75f;
}

ATankPawn::ATankPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 根碰撞盒：根据 1:1 真实尺寸（长7.6m, 宽3.5m, 高2.4m）
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	// 高度 240→236：盒底相对车体抬升 2cm 安全间隙——与齐平路面/地板共面时避免毫米级穿透导致扫掠卡死
	CollisionBox->SetBoxExtent(FVector(380.0f, 175.0f, 118.0f));
	// Pawn profile：Pawn 对 Pawn 默认 Block——FFA 坦克互挡靠它
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	CollisionBox->SetSimulatePhysics(false);
	RootComponent = CollisionBox;

	// 联机（M1 客户端权威移动）：Pawn 基类已 bReplicates=true；
	// 移动复制让服务器收到的位姿转发给其他端。
	// 本机位姿上报见 Tick 的 ServerSyncTransform——不开它，服务器永远停在出生点会把客户端拉回原地
	SetReplicateMovement(true);

	// 1b. 坦克血量组件（M0 挂载；M2 联机升级 Replicated）
	TankHealth = CreateDefaultSubobject<UTankHealth>(TEXT("TankHealth"));

	// 2. 车身底盘 Mesh（底部对齐碰撞盒底面 Z=-120）
	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(CollisionBox);
	HullMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -120.0f));
	HullMesh->SetRelativeScale3D(FVector::OneVector);
	HullMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a_hull_body.ztz88a_hull_body"));
	if (HullMeshAsset.Succeeded())
	{
		HullMesh->SetStaticMesh(HullMeshAsset.Object);
	}

	// 3. 履带 Mesh（与底盘同原点同基准，严丝合缝包裹负重轮）
	TracksMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracksMesh"));
	TracksMesh->SetupAttachment(HullMesh);
	TracksMesh->SetRelativeLocation(FVector::ZeroVector);
	TracksMesh->SetRelativeScale3D(FVector::OneVector);
	TracksMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TracksMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a_tracks_full.ztz88a_tracks_full"));
	if (TracksMeshAsset.Succeeded())
	{
		TracksMesh->SetStaticMesh(TracksMeshAsset.Object);
	}

	// 3b. 负重轮（每侧 6 个，位于履带环内）——由 split_tank_mesh.py 从车体网格拆出
	// 布局来自脚本输出：loc=(X, ±147.72, 41.75)，滚动半径 35cm（相对 TracksMesh 坐标系）
	struct FRoadWheelSetup
	{
		const TCHAR* MeshPath;
		float X;
		bool bRightSide;
	};
	const FRoadWheelSetup RoadSetups[] =
	{
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r0.ztz88a_road_r0"), -206.00f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r1.ztz88a_road_r1"), -131.00f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r2.ztz88a_road_r2"),  -56.00f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r3.ztz88a_road_r3"),   19.00f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r4.ztz88a_road_r4"),  107.50f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_r5.ztz88a_road_r5"),  199.95f, true },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l0.ztz88a_road_l0"), -206.00f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l1.ztz88a_road_l1"), -131.00f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l2.ztz88a_road_l2"),  -56.00f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l3.ztz88a_road_l3"),   19.00f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l4.ztz88a_road_l4"),  107.50f, false },
		{ TEXT("/Game/tank/ztz-88a/ztz88a_road_l5.ztz88a_road_l5"),  199.95f, false },
	};
	RoadWheels.Reserve(UE_ARRAY_COUNT(RoadSetups));
	RoadWheelAngles.Init(0.0f, UE_ARRAY_COUNT(RoadSetups));
	for (int32 i = 0; i < UE_ARRAY_COUNT(RoadSetups); ++i)
	{
		UStaticMeshComponent* WheelComp = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("RoadWheel%d"), i));
		WheelComp->SetupAttachment(TracksMesh);
		WheelComp->SetRelativeLocation(FVector(RoadSetups[i].X,
			RoadSetups[i].bRightSide ? RoadWheelY : -RoadWheelY, RoadWheelZ));
		WheelComp->SetCollisionProfileName(TEXT("NoCollision"));

		ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMeshAsset(RoadSetups[i].MeshPath);
		if (WheelMeshAsset.Succeeded())
		{
			WheelComp->SetStaticMesh(WheelMeshAsset.Object);
		}
		RoadWheels.Add(WheelComp);
	}

	// 3c. 端轮说明：主动轮/诱导轮盘体在车体网格内、被履带包绕覆盖，不做独立
	// 旋转组件（此前尝试拆出旋转端轮会带出履带弧块，见 split_tank_mesh.py 注释）

	// 4. 炮塔水平旋转轴心 (挂在车身上，坐标：X=-2.5, Y=0, Z=145.5)
	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(HullMesh);
	TurretPivot->SetRelativeLocation(FVector(-2.5f, 0.0f, 145.5f));
	TurretPivot->SetRelativeScale3D(FVector::OneVector);

	// 5. 炮塔 Mesh（完整包含铸造炮塔、五角星侧装甲裙框、高射机枪与指挥塔）
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(TurretPivot);
	TurretMesh->SetRelativeLocation(FVector::ZeroVector);
	TurretMesh->SetRelativeRotation(FRotator::ZeroRotator);
	TurretMesh->SetRelativeScale3D(FVector::OneVector);
	TurretMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TurretMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a-turret.ztz88a-turret"));
	if (TurretMeshAsset.Succeeded())
	{
		TurretMesh->SetStaticMesh(TurretMeshAsset.Object);
	}

	// 6. 主炮俯仰轴心 (挂在炮塔轴心上，相对于炮塔基准点坐标：X=170.0, Y=0, Z=31.0)
	GunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("GunPivot"));
	GunPivot->SetupAttachment(TurretPivot);
	GunPivot->SetRelativeLocation(FVector(170.0f, 0.0f, 31.0f));
	GunPivot->SetRelativeScale3D(FVector::OneVector);

	// 7. 主炮 Mesh
	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(GunPivot);
	GunMesh->SetRelativeLocation(FVector::ZeroVector);
	GunMesh->SetRelativeRotation(FRotator::ZeroRotator);
	GunMesh->SetRelativeScale3D(FVector::OneVector);
	GunMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GunMeshAsset(TEXT("/Game/tank/ztz-88a/ztz88a-gun.ztz88a-gun"));
	if (GunMeshAsset.Succeeded())
	{
		GunMesh->SetStaticMesh(GunMeshAsset.Object);
	}

	// 8. 现代商业标杆视口弹簧臂（锁定舒适俯视角，彻底杜绝翻转眩晕，仅绕车体水平回旋）
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CollisionBox);
	SpringArm->TargetArmLength = 950.0f;
	SpringArm->SetRelativeLocation(FVector(-50.0f, 0.0f, 220.0f));
	SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true; // 核心修复：继承底盘偏航，WA转向时视口自然跟随车身切入弯道！
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.0f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 8.0f; // 优雅的旋转滞后平滑，营造重型载具厚重感

	// 9. 摄像机
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// 【M2 重生占有链路 bug 根因】必须 Disabled，占有权完全交给 BattleGameMode.RestartPlayer。
	//
	// 引擎 APawn::PreInitializeComponents 里有：AutoPossessPlayer != Disabled 且非 NM_Client 时
	// 取 GetPlayerController(this, Index) 直接 PC->Possess(this)。而 AController::OnPossess 在
	// 换成另一个 Pawn 时会先 UnPossess() 掉当前 Pawn。
	//
	// 于是每一次「服务端为客户端重生而 Spawn 坦克」都会顺带把 Player0（主机）的 PC 抢过来，
	// 主机正在开的坦克被就地 UnPossess → 变孤儿。连锁反应：
	//   1. 主机坦克失去 Controller（日志里的"神秘 UnPossession"）
	//   2. 巡检只补 GetPawn()==null 的 PC，而主机 PC 此刻正占着别人的新坦克，于是不补发
	//   3. 孤儿坦克没有 Controller，被 ChoosePlayerStart 的存活坦克统计排除
	//      → 新坦克按"离存活坦克最远"选点，正好叠在孤儿坦克身上（堆叠症状）
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	// 配套关闭 AI 自动占有：引擎里 AutoPossessPlayer 一旦为 Disabled，就会放开下面这条分支
	//   AutoPossessPlayer == Disabled && AutoPossessAI != Disabled && GetController() == nullptr
	// 而 APawn 的 AutoPossessAI 默认是 PlacedInWorld，AIControllerClass 默认解析为
	// /Script/AIModule.AIController。开局阶段（World->bStartup 为真）出生会被判定成
	// "PlacedInWorld" → SpawnDefaultController() 生一个 AIController 来抢占有。
	// 本项目的坦克只归玩家控制，显式 Disabled 把这条路彻底堵死。
	AutoPossessAI = EAutoPossessAI::Disabled;

	ProjectileClass = ATankProjectile::StaticClass();

	// 10. Enhanced Input：动作与映射上下文资产位于 /Game/tank/inputs/（MCP 创建）
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCAsset(TEXT("/Game/tank/inputs/IMC_Tank.IMC_Tank"));
	if (IMCAsset.Succeeded())
	{
		DefaultMappingContext = IMCAsset.Object;
	}

	const std::pair<const TCHAR*, TObjectPtr<UInputAction>*> InputAssets[] =
	{
		{ TEXT("/Game/tank/inputs/IA_MoveForward.IA_MoveForward"), &MoveForwardAction },
		{ TEXT("/Game/tank/inputs/IA_MoveBackward.IA_MoveBackward"), &MoveBackwardAction },
		{ TEXT("/Game/tank/inputs/IA_TurnRight.IA_TurnRight"), &TurnRightAction },
		{ TEXT("/Game/tank/inputs/IA_TurnLeft.IA_TurnLeft"), &TurnLeftAction },
		{ TEXT("/Game/tank/inputs/IA_TurretCW.IA_TurretCW"), &TurretCWAction },
		{ TEXT("/Game/tank/inputs/IA_TurretCCW.IA_TurretCCW"), &TurretCCWAction },
		{ TEXT("/Game/tank/inputs/IA_CameraYaw.IA_CameraYaw"), &CameraYawAction },
		{ TEXT("/Game/tank/inputs/IA_GunPitch.IA_GunPitch"), &GunPitchAction },
		{ TEXT("/Game/tank/inputs/IA_Fire.IA_Fire"), &FireAction },
	};
	for (const auto& InputAsset : InputAssets)
	{
		ConstructorHelpers::FObjectFinder<UInputAction> ActionAsset(InputAsset.first);
		if (ActionAsset.Succeeded())
		{
			*InputAsset.second = ActionAsset.Object;
		}
	}
}

void ATankPawn::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// 核心架构自愈机制：强制保证编辑期与运行期拓扑及相对位姿一致性
	if (TurretPivot && GunPivot)
	{
		GunPivot->AttachToComponent(TurretPivot, FAttachmentTransformRules::KeepRelativeTransform);
		GunPivot->SetRelativeLocation(FVector(170.0f, 0.0f, 31.0f));
		GunPivot->SetRelativeScale3D(FVector::OneVector);
	}

	if (GunMesh && GunPivot)
	{
		GunMesh->AttachToComponent(GunPivot, FAttachmentTransformRules::KeepRelativeTransform);
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
		GunMesh->SetRelativeScale3D(FVector::OneVector);
	}
}

void ATankPawn::BeginPlay()
{
	Super::BeginPlay();

	// 1. 初始化动态履带材质实例
	if (TracksMesh)
	{
		if (TracksMesh->GetNumMaterials() > 0)
		{
			TrackMaterialInstance = TracksMesh->CreateAndSetMaterialInstanceDynamic(0);
		}
		else
		{
			UE_LOG(LogTank, Warning, TEXT("TracksMesh has no materials assigned. Track UV animation disabled."));
		}
	}

	// 2. 初始化相机朝向与火炮仰角
	CameraRelativeYaw = 0.0f;
	DesiredGunPitch = 0.0f;
	CurrentPitch = 0.0f;

	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, CameraRelativeYaw, 0.0f));
	}

	UE_LOG(LogTank, Log, TEXT("TankPawn initialized with Vehicle-Relative Camera and Direct Gun Elevation."));

	// 3. M3 重生保护：每次出生（含首次）短暂无敌，防落地秒杀。
	//    只由服务器置位，客户端通过复制拿到状态给 HUD 用
	if (HasAuthority() && SpawnProtectionDuration > 0.0f)
	{
		bSpawnProtected = true;
		GetWorldTimerManager().SetTimer(SpawnProtectionTimerHandle, this,
			&ATankPawn::ClearSpawnProtection, SpawnProtectionDuration, false);
		UE_LOG(LogTank, Log, TEXT("[Battle] %s 进入重生保护 %.1fs"), *GetName(), SpawnProtectionDuration);
	}

	AddDefaultMappingContext();
	EnsureClientReady();
}

void ATankPawn::PostNetReceive()
{
	Super::PostNetReceive();

	// 客户端每收到一次复制更新都会走这里（含首次）——由 FObjectReplicator::PostReceivedBunch
	// 在 bHasReplicatedProperties 时调用，是常规复制路径；不像 PostNetInit 只在
	// DataChannel/DemoNetDriver 才走。客户端 Pawn 的 BeginPlay 可能早于复制状态到达，
	// 而 Tick 注册与输入绑定只挂在引擎那两条一次性路径上（PawnClientRestart 里，
	// 且仅在 InputComponent == nullptr 时才绑）。PIE 三开实测：客户端世界的坦克
	// Tick 注册 0 / 启用 0、InputComponent 为空、动作未绑、IMC 未挂 —— 输入回调
	// （不走 Tick）活着、Tick 侧（位移/炮塔/视口/轮子）是死的，表现即「只能开炮不能移动」。
	// 幂等由 EnsureClientReady 内部的 bTickRepaired / bClientReady 两个闸门保证。
	EnsureClientReady();
}

void ATankPawn::EnsureClientReady()
{
	if (HasAuthority())
	{
		return;
	}

	// ---------- 1. Tick 自愈：客户端世界里的「每一辆」坦克都要做 ----------
	//
	// 客户端世界的坦克全是网络生成，Tick 函数没被注册（PIE 实测：Tick 注册 0 / 启用 0）。
	// 这不只影响自己的车：炮塔与火炮朝向（Tick 里 CurrentTurretYaw = NetTurretYaw →
	// TurretPivot->SetRelativeRotation）、后坐力复位、以及第 8 段「从实际位移反推负重轮转速」
	// 全都在 Tick 里落地。实测另一客户端看这辆车：net_turret_yaw 已复制到 102.1、
	// current_turret_yaw 仍是 0 —— 复制值到了但 TurretPivot 不动，炮塔就是不转。
	// 所以这一段不做归属判断，先把 Tick 修好。RegisterTickFunction 自带幂等闸门。
	if (!bTickRepaired)
	{
		bTickRepaired = true;
		const bool bWasRegistered = PrimaryActorTick.IsTickFunctionRegistered();
		const bool bWasEnabled = IsActorTickEnabled();
		if (!bWasRegistered)
		{
			RegisterAllActorTickFunctions(true, /*bDoComponents=*/false);
		}
		if (!bWasEnabled)
		{
			SetActorTickEnabled(true);
		}
		if (!bWasRegistered || !bWasEnabled)
		{
			UE_LOG(LogTank, Log, TEXT("[Net] %s Tick 自愈：注册 %d→%d / 启用 %d→%d（Role=%d）"),
				*GetName(),
				bWasRegistered ? 1 : 0, PrimaryActorTick.IsTickFunctionRegistered() ? 1 : 0,
				bWasEnabled ? 1 : 0, IsActorTickEnabled() ? 1 : 0,
				(int32)GetLocalRole());
		}
	}

	if (bClientReady)
	{
		return;
	}

	// ---------- 2. 输入自愈：只给「本机玩家的本机 Pawn」 ----------
	// 远端坦克在本端没有 LocalPlayer 子系统，既不需要输入，也不能白建 InputComponent
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		// 客户端的占有靠 Controller 复制到达，OnRep_Controller 会再调一次
		return;
	}
	if (!ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		return;
	}
	bClientReady = true;

	// 记账：只在真的修了什么的时候才动引擎状态，日志也据此如实汇报（便于下一轮判定根因）
	const bool bHadContext = bMappingContextAdded;
	bool bCreatedInput = false;
	bool bBoundInput = false;

	// 输入组件与绑定：引擎只在 PawnClientRestart 里、且仅在 InputComponent == nullptr 时
	// 调 SetupPlayerInputComponent；这条路径一旦被跳过，本机坦克收不到任何动作
	if (InputComponent == nullptr)
	{
		InputComponent = CreatePlayerInputComponent();
		if (InputComponent)
		{
			InputComponent->RegisterComponent();
			bCreatedInput = true;
		}
	}
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// 已有绑定就不重复绑（重复绑定会让每个动作回调触发两次）
		if (EnhancedInput->GetActionEventBindings().Num() == 0)
		{
			SetupPlayerInputComponent(InputComponent);
			bBoundInput = true;
		}
	}

	// 映射上下文（内部自带幂等闸门）
	AddDefaultMappingContext();

	// 单行汇报本次自愈的实际动作：若客户端一切正常，这行会显示「本来就是好的」，
	// 也就等效于证伪了「引擎漏注册」这一假设，把矛头指向输入投递
	UE_LOG(LogTank, Log, TEXT("[Input] %s 输入自愈：输入组件%s，动作绑定%s，IMC%s（Role=%d）"),
		*GetName(),
		bCreatedInput ? TEXT("补建") : TEXT("已在"),
		bBoundInput ? TEXT("补绑") : TEXT("已在"),
		bHadContext ? TEXT("已挂") : TEXT("新挂"),
		(int32)GetLocalRole());
}

void ATankPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 重生保护状态：公开信息，所有端都要看到（HUD 显示 + 远端表现）
	DOREPLIFETIME(ATankPawn, bSpawnProtected);

	// 炮塔/火炮朝向：给「其他端」看的；拥有者本地自算，不必回传（COND_SkipOwner）
	DOREPLIFETIME_CONDITION(ATankPawn, NetTurretYaw, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ATankPawn, NetGunPitch, COND_SkipOwner);
}

void ATankPawn::ClearSpawnProtection()
{
	if (!HasAuthority())
	{
		return;
	}
	bSpawnProtected = false;
	OnRep_SpawnProtected(); // 服务器本地不走 OnRep，手动补一次
}

void ATankPawn::OnRep_SpawnProtected()
{
	UE_LOG(LogTank, Verbose, TEXT("[Battle] %s 重生保护 = %s"),
		*GetName(), bSpawnProtected ? TEXT("ON") : TEXT("OFF"));
}

void ATankPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// 服务器生成坦克的顺序是 Spawn（触发 BeginPlay，此时 Controller 为空）→ Possess。
	// 映射上下文若只在 BeginPlay 挂，主机（服务器本地玩家）永远挂不上、输入失灵
	AddDefaultMappingContext();
	EnsureClientReady();
}

void ATankPawn::UnPossessed()
{
	Super::UnPossessed();
	// 占有链路自检埋点：根因修复后这里应只在「死亡销毁」时出现。
	// 若战斗中再次刷出非死亡触发的 UnPossessed，看 Frame/PendingKill 即可判定是引擎回收还是被抢占有
	UE_LOG(LogTank, Log, TEXT("[Input] %s 被 UnPossessed！t=%.1f Frame=%llu PendingKill=%d"),
		*GetName(),
		GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f,
		GFrameCounter,
		IsPendingKillPending() ? 1 : 0);
}

void ATankPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	// 客户端的 Possess 不会跨网调用，用 Controller 复制回调兜底
	AddDefaultMappingContext();
	// 客户端的本机 Pawn 到这一刻才真正拿到 Controller，是补 Tick/绑定的最佳时机
	EnsureClientReady();
}

void ATankPawn::AddDefaultMappingContext()
{
	if (bMappingContextAdded || !DefaultMappingContext)
	{
		return;
	}
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogTank, Warning, TEXT("[Input] %s 挂上下文失败：无 PC（Role=%d）"), *GetName(), (int32)GetLocalRole());
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogTank, Warning, TEXT("[Input] %s 挂上下文失败：无 LocalPlayer 子系统"), *GetName());
		return;
	}
	Subsystem->AddMappingContext(DefaultMappingContext, 0);
	bMappingContextAdded = true;
	UE_LOG(LogTank, Log, TEXT("[Input] %s 上下文已挂载（Role=%d）"), *GetName(), (int32)GetLocalRole());
}

void ATankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// M1 客户端权威移动同步：本机客户端模拟 → 50Hz 上报服务器 →
	// 服务器 SetActorLocationAndRotation 后经移动复制转发其他端。
	// 主机端（Authority）直接本地模拟，无需上报
	if (IsLocallyControlled() && !HasAuthority())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastTransformSyncTime >= TransformSyncInterval)
		{
			LastTransformSyncTime = Now;
			// 炮塔/火炮朝向随位姿一起上报：炮塔朝向是本机状态，服务器原本完全不知道，
			// 于是敌方视角只看到车体在转、炮塔永远朝车头（见 NetTurretYaw 注释）
			ServerSyncTransform(GetActorLocation(), GetActorRotation(), CurrentTurretYaw, CurrentPitch);
		}
	}

	// 0.9 被推位移平滑消化：推挤 RPC 按 推动 方帧率到达，节奏差会造成跳帧；
	// 累积进 PendingPushOffset 后按恒定速度消耗（sweep 撞墙即弃剩余量，推动方下帧的推挤会继续跟进）
	if (!PendingPushOffset.IsNearlyZero())
	{
		const FVector Step = PendingPushOffset.GetClampedToMaxSize(PushConsumeSpeed * DeltaTime);
		FHitResult PushHit;
		SetActorLocation(GetActorLocation() + Step, true, &PushHit);
		PendingPushOffset = PushHit.bBlockingHit ? FVector::ZeroVector : PendingPushOffset - Step;
	}

	// 1. WASD 前后行进
	if (!FMath::IsNearlyZero(CurrentMoveInput))
	{
		const FVector MoveDelta = FVector(CurrentMoveInput * MoveSpeed * DeltaTime, 0.0f, 0.0f);
		FHitResult Hit;
		const FVector BeforeLoc = GetActorLocation();
		AddActorLocalOffset(MoveDelta, true, &Hit);

		// 每个实例只打一次：证伪/证实「输入→Tick→位移」这条链在本机是通的。
		// 客户端不动时，有没有这一行即可区分「输入没到」和「Tick 没跑 / 被几何卡死」
		if (!bLoggedFirstLocalMove && IsLocallyControlled())
		{
			bLoggedFirstLocalMove = true;
			UE_LOG(LogTank, Log, TEXT("[Input] %s 本机位移首次生效：本帧 %.1fcm（被阻挡=%d）"),
				*GetName(), FVector::Dist(BeforeLoc, GetActorLocation()), Hit.bBlockingHit ? 1 : 0);
		}

		// M1.5 挤压推进：本机扫掠被对方坦克挡住 → 沿本机推进方向把对方顶开
		if (Hit.bBlockingHit)
		{
			if (ATankPawn* HitTank = Cast<ATankPawn>(Hit.GetActor()))
			{
				TryPushTank(HitTank, GetActorRotation().RotateVector(MoveDelta) * PushStrength);
			}
		}

		if (TrackMaterialInstance)
		{
			TrackUVOffset += CurrentMoveInput * (MoveSpeed / FMath::Max(1.0f, TrackUVSpeedScale)) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 2. WASD 原地差速掉头（坦克约定：A/D 恒定转向车体，与前后进无关，倒车不反向）
	if (!FMath::IsNearlyZero(CurrentTurnInput))
	{
		const FRotator TurnDelta = FRotator(0.0f, CurrentTurnInput * TurnSpeed * DeltaTime, 0.0f);
		AddActorLocalRotation(TurnDelta, true);

		if (TrackMaterialInstance && FMath::IsNearlyZero(CurrentMoveInput))
		{
			TrackUVOffset += CurrentTurnInput * (TurnSpeed / FMath::Max(1.0f, TrackUVSpeedScale * 0.1f)) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 3. 视口跟随：继承车身偏航并叠加相对偏航角 CameraRelativeYaw，Pitch锁定在舒适俯角
	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(FixedCameraPitch, CameraRelativeYaw, 0.0f));
	}

	// 4. 直线行驶自动回正（如启用）
	if (bEnableAutoCenter && !FMath::IsNearlyZero(CurrentMoveInput) && FMath::IsNearlyZero(CurrentTurnInput))
	{
		CameraRelativeYaw = FMath::FInterpTo(CameraRelativeYaw, 0.0f, DeltaTime, AutoCenterSpeed);
	}

	// 5~6. 炮塔水平伺服 / 主炮高低机
	//
	// 本机受控（含主机自己的车）→ 由输入驱动伺服算出权威值；
	// 其余实例（本机看到的他机、服务器上客户端那辆）→ 直接吃复制值。
	// 关键：不能让远端跑伺服——那边 CameraRelativeYaw / DesiredGunPitch 恒为 0，
	// 伺服会把炮塔一路拽回车头、火炮压回 0°，这正是「敌方视角看不到炮塔转向」的原因。
	if (IsLocallyControlled())
	{
		// 5. 炮塔水平电驱伺服追踪（追赶玩家视线相对偏航角 CameraRelativeYaw）
		if (!FMath::IsNearlyZero(CurrentTurretRotateInput))
		{
			CameraRelativeYaw = FRotator::NormalizeAxis(CameraRelativeYaw + CurrentTurretRotateInput * TurretRotateSpeed * DeltaTime);
		}
		CurrentTurretYaw = FMath::FixedTurn(CurrentTurretYaw, CameraRelativeYaw, TurretRotateSpeed * DeltaTime);

		// 6. 主炮垂直电驱高低机平滑追踪：ElevateGun 直接改 DesiredGunPitch，这里只负责恒速靠拢
		CurrentPitch = FMath::FInterpConstantTo(CurrentPitch, DesiredGunPitch, DeltaTime, PitchSpeed);
	}
	else
	{
		// 复制值本身就是对端伺服后的结果，直接用（再插值只会引入额外滞后）
		CurrentTurretYaw = NetTurretYaw;
		CurrentPitch = NetGunPitch;
	}

	if (TurretPivot)
	{
		TurretPivot->SetRelativeRotation(FRotator(0.0f, CurrentTurretYaw, 0.0f));
	}
	if (GunPivot)
	{
		GunPivot->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	}

	// 7. 主炮后坐力渐进平滑复位
	if (GunMesh)
	{
		if (CurrentRecoilOffset < -0.01f)
		{
			CurrentRecoilOffset = FMath::FInterpTo(CurrentRecoilOffset, 0.0f, DeltaTime, RecoilRecoverySpeed);
		}
		else
		{
			CurrentRecoilOffset = 0.0f;
		}
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}

	// 8. 负重轮差速旋转（+Yaw 为右转：右履带减速、左履带加速；轮角速度=履带线速度/滚动半径）
	//
	// 两条路径，因为「输入」只在本机受控实例上非零：
	//   本机受控 → 直接用输入算，零延迟
	//   其他（服务器上的他机 / 客户端的远端坦克）→ 输入恒为 0，必须从实际位移反推，
	//   否则远端坦克会"车在滑行、轮子不转"
	const float HalfSpan = FMath::Max(1.0f, TrackSpan) * 0.5f;

	if (IsLocallyControlled())
	{
		if (!FMath::IsNearlyZero(CurrentMoveInput) || !FMath::IsNearlyZero(CurrentTurnInput))
		{
			const float YawRateRad = FMath::DegreesToRadians(CurrentTurnInput * TurnSpeed);
			UpdateWheelSpin(DeltaTime,
				CurrentMoveInput * MoveSpeed + YawRateRad * HalfSpan,
				CurrentMoveInput * MoveSpeed - YawRateRad * HalfSpan);
		}
	}
	else
	{
		const float SafeDt = FMath::Max(DeltaTime, 0.0001f);
		const FVector CurLoc = GetActorLocation();
		const float CurYaw = GetActorRotation().Yaw;

		if (bHasLastTickPose)
		{
			// 前向速度：位移在本车前进轴上的投影 / dt（客户端上引擎已做网络平滑，所以是平滑的）
			const float ForwardSpeed = FVector::DotProduct(CurLoc - LastTickLocation, GetActorForwardVector()) / SafeDt;
			// 偏航角速度：Yaw 差 / dt（NormalizeAxis 处理 ±180 环绕）
			const float YawRateRad = FMath::DegreesToRadians(FRotator::NormalizeAxis(CurYaw - LastTickYaw) / SafeDt);

			// 网络抖动/瞬时大步距可能算出离谱值，夹一下防止轮子疯转
			const float Clamped = FMath::Clamp(ForwardSpeed, -MoveSpeed * 1.5f, MoveSpeed * 1.5f);
			UpdateWheelSpin(DeltaTime,
				Clamped + YawRateRad * HalfSpan,
				Clamped - YawRateRad * HalfSpan);
		}

		LastTickLocation = CurLoc;
		LastTickYaw = CurYaw;
		bHasLastTickPose = true;
	}

	// 9. 真实弹道指向与射击点（从炮口沿炮管轴线探测，鼠标纵向调节时清晰上下位移）
	if (bDrawAimDebug && GetWorld() && GunMesh)
	{
		const FVector MuzzleLoc = GunMesh->GetComponentTransform().TransformPosition(FVector(MuzzleForwardOffset, 0.0f, 0.0f));
		const FVector GunForward = GunMesh->GetForwardVector();
		const FVector GunTraceEnd = MuzzleLoc + GunForward * MaxAimDistance;

		FHitResult GunHit;
		FCollisionQueryParams GunQueryParams(TEXT("TankGunTrace"), false, this);
		GunQueryParams.AddIgnoredActor(this);

		FVector ActualAimPoint = GunTraceEnd;
		if (GetWorld()->LineTraceSingleByChannel(GunHit, MuzzleLoc, GunTraceEnd, ECC_Visibility, GunQueryParams))
		{
			ActualAimPoint = GunHit.ImpactPoint;
		}

		// 绘制实际射击着弹点与主炮指向射线
		DrawDebugSphere(GetWorld(), ActualAimPoint, 35.0f, 16, FColor::Yellow, false, -1.0f, 0, 2.5f);
		DrawDebugLine(GetWorld(), MuzzleLoc, MuzzleLoc + GunForward * 1200.0f, FColor::Orange, false, -1.0f, 0, 2.0f);
	}

	CurrentMoveInput = 0.0f;
	CurrentTurnInput = 0.0f;
	CurrentTurretRotateInput = 0.0f;
}

void ATankPawn::UpdateWheelSpin(float DeltaTime, float TrackSpeedLeft, float TrackSpeedRight)
{
	const float SpinSign = bInvertWheelSpin ? -1.0f : 1.0f;

	// 负重轮：0-5 为右侧、6-11 为左侧，与所在侧履带线速度一致
	for (int32 i = 0; i < RoadWheels.Num(); ++i)
	{
		const float SideSpeed = (i < 6) ? TrackSpeedRight : TrackSpeedLeft;
		RoadWheelAngles[i] = FMath::Fmod(
			RoadWheelAngles[i] + SpinSign * FMath::RadiansToDegrees(SideSpeed / RoadWheelRollRadius) * DeltaTime,
			360.0f);
		if (RoadWheels[i])
		{
			RoadWheels[i]->SetRelativeRotation(FRotator(RoadWheelAngles[i], 0.0f, 0.0f));
		}
	}
}

void ATankPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTank, Error, TEXT("Missing EnhancedInputComponent — check DefaultInput.ini DefaultInputComponentClass."));
		return;
	}

	EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ATankPawn::MoveForward);
	EnhancedInput->BindAction(MoveBackwardAction, ETriggerEvent::Triggered, this, &ATankPawn::MoveBackward);
	EnhancedInput->BindAction(TurnRightAction, ETriggerEvent::Triggered, this, &ATankPawn::TurnRight);
	EnhancedInput->BindAction(TurnLeftAction, ETriggerEvent::Triggered, this, &ATankPawn::TurnLeft);
	EnhancedInput->BindAction(TurretCWAction, ETriggerEvent::Triggered, this, &ATankPawn::TurretCW);
	EnhancedInput->BindAction(TurretCCWAction, ETriggerEvent::Triggered, this, &ATankPawn::TurretCCW);
	EnhancedInput->BindAction(CameraYawAction, ETriggerEvent::Triggered, this, &ATankPawn::OrbitCamera);
	EnhancedInput->BindAction(GunPitchAction, ETriggerEvent::Triggered, this, &ATankPawn::ElevateGun);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ATankPawn::Fire);
}

void ATankPawn::MoveForward()
{
	// 逐帧触发（按住 W 每帧一次），必须 Verbose——Log 级会以每秒上百行淹没日志
	UE_LOG(LogTank, Verbose, TEXT("[Input] %s MoveForward 触发（Role=%d）"), *GetName(), (int32)GetLocalRole());
	CurrentMoveInput = 1.0f;
}

void ATankPawn::MoveBackward()
{
	CurrentMoveInput = -1.0f;
}

void ATankPawn::TurnRight()
{
	CurrentTurnInput = 1.0f;
}

void ATankPawn::TurnLeft()
{
	CurrentTurnInput = -1.0f;
}

void ATankPawn::TurretCW()
{
	CurrentTurretRotateInput = 1.0f;
}

void ATankPawn::TurretCCW()
{
	CurrentTurretRotateInput = -1.0f;
}

void ATankPawn::OrbitCamera(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (!FMath::IsNearlyZero(Axis))
	{
		CameraRelativeYaw = FRotator::NormalizeAxis(CameraRelativeYaw + Axis * CameraSensitivityX);
	}
}

void ATankPawn::ElevateGun(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (!FMath::IsNearlyZero(Axis))
	{
		// 鼠标向上推 -> 仰角增大(向上抬)；鼠标向下拉 -> 仰角减小(向下压)
		const float Direction = bInvertPitch ? -1.0f : 1.0f;
		DesiredGunPitch = FMath::Clamp(DesiredGunPitch + Axis * PitchSensitivity * Direction, MinPitch, MaxPitch);
	}
}

void ATankPawn::Fire()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 阵亡到销毁之间有 0.2s 窗口，期间 Pawn 还活着但已不该开火
	if (TankHealth && TankHealth->IsDepleted())
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastFireTime < FireCooldown)
	{
		UE_LOG(LogTank, Verbose, TEXT("[Tank] Gun reloading: %.2f / %.2fs"), (CurrentTime - LastFireTime), FireCooldown);
		return;
	}

	LastFireTime = CurrentTime;

	// 1. 触发主炮液压后坐力冲压（本机表现即时预测，其他端由 MulticastFireFX 补齐）
	CurrentRecoilOffset = RecoilDistance;
	if (GunMesh)
	{
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}

	// 2. 炮口世界位姿（炮塔俯仰是本机状态，服务器不知道——随 RPC 上报，灰盒接受其作弊面）
	const FVector MuzzleLoc = GunMesh ? GunMesh->GetComponentTransform().TransformPosition(FVector(MuzzleForwardOffset, 0.0f, 0.0f)) : GetActorLocation();
	const FVector AimDir = GunMesh ? GunMesh->GetComponentRotation().Vector() : GetActorRotation().Vector();

	// 3. 炮弹生成服务器独占（双端生成会双重伤害）
	if (HasAuthority())
	{
		ExecuteFire(MuzzleLoc, AimDir);
	}
	else
	{
		ServerFire(MuzzleLoc, AimDir);
	}

	// 4. 炮口瞬时开火闪光/示踪线（本机即时，其他端由 Multicast 补）
	DrawDebugLine(World, MuzzleLoc, MuzzleLoc + AimDir * 3000.0f, FColor::Yellow, false, 0.15f, 0, 4.0f);
}

void ATankPawn::ExecuteFire(const FVector& MuzzleLoc, const FVector& AimDir)
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ATankProjectile::StaticClass();
	World->SpawnActor<ATankProjectile>(ClassToSpawn, MuzzleLoc, AimDir.Rotation(), SpawnParams);

	UE_LOG(LogTank, Log, TEXT("[Battle] %s 开火 @ (%.0f,%.0f,%.0f)"),
		*GetName(), MuzzleLoc.X, MuzzleLoc.Y, MuzzleLoc.Z);
}

bool ATankPawn::ServerFire_Validate(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	const FVector Loc(MuzzleLoc);
	const FVector Dir(AimDir);
	return !Loc.ContainsNaN() && !Dir.ContainsNaN() && !Dir.IsNearlyZero();
}

void ATankPawn::ServerFire_Implementation(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	// 服务器侧宽松射速限制（容忍网络延迟下的合法连发，拦截无脑刷弹）
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < FireCooldown * 0.5f)
	{
		return;
	}
	LastFireTime = Now;

	ExecuteFire(FVector(MuzzleLoc), FVector(AimDir));
	MulticastFireFX(MuzzleLoc, AimDir);
}

void ATankPawn::MulticastFireFX_Implementation(FVector_NetQuantize100 MuzzleLoc, FVector_NetQuantizeNormal AimDir)
{
	// 远端后坐力动画（所有者本机已在 Fire() 里预测过，重复置位无害）
	CurrentRecoilOffset = RecoilDistance;
	if (GunMesh)
	{
		GunMesh->SetRelativeLocation(FVector(CurrentRecoilOffset, 0.0f, 0.0f));
	}
	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, MuzzleLoc, FVector(MuzzleLoc) + FVector(AimDir) * 3000.0f, FColor::Yellow, false, 0.15f, 0, 4.0f);
	}
}

void ATankPawn::MulticastDeathFX_Implementation(FVector_NetQuantize100 DeathLoc)
{
	if (UWorld* World = GetWorld())
	{
		// 灰盒爆炸表达：橙红双球驻留 2s（P4 换 Niagara）
		//
		// 注意 bPersistentLines 必须为 false：它为 true 时表示"持久线，直到 FlushPersistentDebugLines
		// 才清"，LifeTime 不生效——实测爆炸球会一直挂在残骸上（刚重生的车看起来像套在爆炸里）。
		// 传 false + LifeTime 才是"定时消失"。
		DrawDebugSphere(World, FVector(DeathLoc), 250.0f, 16, FColor::Orange, false, 2.0f, 0, 6.0f);
		DrawDebugSphere(World, FVector(DeathLoc) + FVector(0, 0, 120.0f), 150.0f, 16, FColor::Red, false, 2.0f, 0, 4.0f);
	}
}

void ATankPawn::ClientSetDeathCamera_Implementation(FVector_NetQuantize100 Location, FRotator Rotation)
{
	// AController::SetActorLocation 是 private，外部挪不动 PC；
	// 所以交给 TankPlayerController 覆盖 CalcCamera 真正读的那两个函数
	if (ATankPlayerController* PC = Cast<ATankPlayerController>(GetController()))
	{
		PC->SetDeathViewLocation(FVector(Location), Rotation);
	}
}

float ATankPawn::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 伤害只在服务器结算（炮弹命中判定服务器独占，客户端弹体 OnHit 已被门挡掉）
	if (!HasAuthority() || DamageAmount <= 0.0f || !TankHealth || TankHealth->IsDepleted())
	{
		return 0.0f;
	}

	// M3 重生保护：保护期内免疫伤害（FFA 混战防落地秒杀）
	if (bSpawnProtected)
	{
		UE_LOG(LogTank, Verbose, TEXT("[Battle] %s 处于重生保护，免疫 %.0f 伤害"), *GetName(), DamageAmount);
		return 0.0f;
	}

	const float Remaining = TankHealth->ApplyDamage(DamageAmount);
	UE_LOG(LogTank, Log, TEXT("[Battle] %s 遭受 %.0f 伤害（余 %.0f/%.0f，来源 %s）"),
		*GetName(), DamageAmount, Remaining, TankHealth->GetMaxHealth(),
		DamageCauser ? *DamageCauser->GetName() : TEXT("?"));

	if (TankHealth->IsDepleted())
	{
		HandleDeath(EventInstigator);
	}
	return DamageAmount;
}

void ATankPawn::HandleDeath(AController* Killer)
{
	const FVector DeathLoc = GetActorLocation();
	UE_LOG(LogTank, Warning, TEXT("[Battle] %s 被击毁（击杀者 %s），2s 后重生"),
		*GetName(), Killer ? *Killer->GetName() : TEXT("?"));

	// M3 击杀结算：规则判定属 GameMode 职责，这里只把「谁杀了谁」上报。
	// 必须先于销毁上报——SetLifeSpan 后本 Actor 很快就不在了
	if (ABattleGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ABattleGameMode>() : nullptr)
	{
		GM->NotifyKill(Killer, GetController());
	}

	// 先广播表现再销毁；SetLifeSpan 留出 Multicast 的发送窗口，销毁后由占有自愈巡检在 2s 内补发新坦克
	MulticastDeathFX(DeathLoc);
	// 让被击杀玩家的镜头停在残骸后上方回看（否则 Pawn 销毁后镜头会掉到原点朝天）。
	// 退后 900 / 抬高 350：正好把残骸放进画面，同时避免镜头卡在爆炸球内部
	const FRotator DeathRot = GetActorRotation();
	const FVector DeathCamLoc = DeathLoc - DeathRot.Vector() * 900.0f + FVector(0.0f, 0.0f, 350.0f);

	// 主机（本地控制的 listen server）显式走本地执行，不依赖引擎对本地 Client RPC 的短路行为
	if (APlayerController* OwnerPC = Cast<APlayerController>(GetController()))
	{
		if (OwnerPC->IsLocalController())
		{
			ClientSetDeathCamera_Implementation(DeathCamLoc, DeathRot);
		}
		else
		{
			ClientSetDeathCamera(DeathCamLoc, DeathRot);
		}
	}
	SetLifeSpan(0.2f);
}

void ATankPawn::ServerSyncTransform_Implementation(const FVector_NetQuantize100& Location, const FRotator& NetRotation,
	float TurretYaw, float GunPitch)
{
	// 信任客户端位姿（M1 无反作弊，见计划书）。不走 sweep：客户端本地已做过碰撞解析，
	// 服务器再 sweep 会因时序差拒绝合法移动；物理状态重置避免与瞬时大步距冲突
	SetActorLocationAndRotation(FVector(Location), NetRotation, false, nullptr, ETeleportType::ResetPhysics);

	// 炮塔/火炮朝向：只当作「表现状态」收下并转发（伤害判定用的是 ServerFire 随包带来的炮口位姿，
	// 不依赖这两个值，所以即使丢包也只是远端炮塔短暂滞后，不会造成不同步的判定）
	NetTurretYaw = TurretYaw;
	NetGunPitch = GunPitch;
}

bool ATankPawn::ServerSyncTransform_Validate(const FVector_NetQuantize100& Location, const FRotator& NetRotation,
	float TurretYaw, float GunPitch)
{
	const FVector Loc(Location);
	return !Loc.ContainsNaN() && !NetRotation.ContainsNaN()
		&& !FMath::IsNaN(TurretYaw) && !FMath::IsNaN(GunPitch);
}

// ==================== M1.5 挤压推进 ====================

void ATankPawn::TryPushTank(ATankPawn* HitTank, const FVector& PushDelta)
{
	if (HasAuthority())
	{
		ExecuteTankPush(HitTank, PushDelta);
	}
	else
	{
		ServerPushTank(HitTank, PushDelta);
	}
}

bool ATankPawn::ServerPushTank_Validate(ATankPawn* PushedTank, FVector_NetQuantize10 PushDelta)
{
	// 宽松校验：拒绝空引用与物理上不可能的单帧推挤量（500cm ≫ 600cm/s 速度在任何合法 dt 下都到不了）
	const FVector Delta(PushDelta);
	return IsValid(PushedTank) && !Delta.ContainsNaN() && Delta.Size() < 500.0f;
}

void ATankPawn::ServerPushTank_Implementation(ATankPawn* PushedTank, FVector_NetQuantize10 PushDelta)
{
	ExecuteTankPush(PushedTank, FVector(PushDelta));
}

void ATankPawn::ClientApplyPush_Implementation(FVector_NetQuantize10 PushDelta)
{
	// Client RPC 只投递给被推端的所有者连接；防御性跳过服务器本机执行路径
	if (!HasAuthority())
	{
		ApplyPushDelta(FVector(PushDelta));
	}
}

void ATankPawn::ExecuteTankPush(ATankPawn* PushedTank, const FVector& PushDelta)
{
	if (!PushedTank || PushedTank == this || PushDelta.IsNearlyZero())
	{
		return;
	}
	// 注意：Listen 服务器上所有复制 Pawn 的 HasAuthority() 都是 true，不能用 HasAuthority 区分归属！
	// IsLocallyControlled 在服务器上仅主机自有坦克为 true
	if (PushedTank->IsLocallyControlled())
	{
		// 被推坦克属于主机：主机即权威，直接应用
		PushedTank->ApplyPushDelta(PushDelta);
	}
	else
	{
		// 被推坦克由客户端权威：通知其所有者本地应用，随其常规位姿上报自动收敛到服务器（无位置打架）
		PushedTank->ClientApplyPush(PushDelta);
	}
}

void ATankPawn::ApplyPushDelta(const FVector& PushDelta)
{
	// 不瞬移：累积进平滑消化队列（上限 300cm 防积压），Tick 按 PushConsumeSpeed 恒速消耗
	PendingPushOffset = (PendingPushOffset + PushDelta).GetClampedToMaxSize(300.0f);
}
