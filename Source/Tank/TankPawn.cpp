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

	// 地形跟随的四角探针内缩量（cm）：贴到碰撞盒角点里面一点，避免角点正好压着别的物体边缘
	constexpr float GroundProbeInset = 10.0f;
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

	// M4b 整车缩放：只缩根组件，所有子组件等比跟缩（见 VehicleScale 的注释）
	CollisionBox->SetRelativeScale3D(FVector(FMath::Max(VehicleScale, 0.01f)));

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
	// 臂长基准值在 CameraArmLength；是否跟整车一起缩放见 ApplyVehicleScale / bScaleCameraWithVehicle
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CollisionBox);
	SpringArm->TargetArmLength = CameraArmLength;
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

	// M4b 整车缩放：构造函数里已经设过一次，这里再兜一次底（幂等），
	// 覆盖「组件在编辑器里被改过缩放 / 蓝图子类改了 VehicleScale」的情况
	ApplyVehicleScale();

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
	// 幂等由 EnsureClientReady 内部闸门保证（Tick 部分每次查，输入部分一次性）。
	EnsureClientReady();

	// 1s 心跳兜底：RunUnderOneProcess 下客户端坦克的 Tick 会在运行中再次丢失，
	// 而上面那些钩子都依赖「有复制/占有事件」—— 没有网络流量时补不到
	// （M4c-3 实测：车悬在掩体棱上、World 时钟照走、车一动不动，投一次键触发复制才自愈）。
	GetWorldTimerManager().SetTimer(TickRepairTimerHandle, this, &ATankPawn::RepairTickTimer, 1.0f, true);
}

void ATankPawn::RepairTickTimer()
{
	// 内部只有「未注册才注册、未启用才启用」的开关判断，开销可忽略
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
	// **不加一次性闸门**：M4c-3 实测 Tick 会在运行中再次丢失（车悬在棱上一动不动、World 时钟照走），
	// 而 RegisterAllActorTickFunctions / SetActorTickEnabled 的开关检查本身很便宜 —— 每次都查。
	{
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

	// ---- M1 位姿上报（★ 升级边界：阶段 3 删除，见 docs/tank-vehicle-upgrade-plan.md）----
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

	// 1. WASD 前后行进（★ 升级边界：位移与滑面在阶段 1/3 交给 Chaos 载具，本段届时删除）
	//    保留：M1.5 挤压推进（TryPushTank）与履带 UV —— 与载具实现无关，升级后按新组件重接
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

		// M4b 爬坡：被「可行走」的面挡住 → 把剩余位移投影到该面继续走（slide）。
		// 坡道 25° 的法线 Z≈0.91 ≥ cos30°≈0.87 → 会沿坡往上滑；
		// 边界台阶是竖直面（法线 Z=0）→ 不满足条件 → 照样挡死，不会翻过边界
		if (Hit.bBlockingHit)
		{
			const float WalkableCos = FMath::Cos(FMath::DegreesToRadians(MaxClimbSlopeDeg));
			if (Hit.ImpactNormal.Z >= WalkableCos)
			{
				// 注意单位：MoveDelta 是局部空间，法线是世界空间 → 先转到世界再投影
				const float ResidualScale = 1.0f - FMath::Clamp(Hit.Time, 0.0f, 1.0f);
				const FVector WorldResidual = GetActorRotation().RotateVector(MoveDelta) * ResidualScale;
				const FVector SlideDelta = FVector::VectorPlaneProject(WorldResidual, Hit.ImpactNormal);
				if (!SlideDelta.IsNearlyZero())
				{
					SetActorLocation(GetActorLocation() + SlideDelta, /*bSweep=*/true);
				}
			}
		}

		if (TrackMaterialInstance)
		{
			// 履带纹理的滚动速度按「世界位移 / 纹理一格对应的世界长度」算：
			// 整车缩了 VehicleScale，同样的世界速度下 UV 要滚得更快才对
			TrackUVOffset += CurrentMoveInput * (MoveSpeed / (FMath::Max(1.0f, TrackUVSpeedScale) * FMath::Max(VehicleScale, 0.01f))) * DeltaTime;
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
			TrackUVOffset += CurrentTurnInput * (TurnSpeed / (FMath::Max(1.0f, TrackUVSpeedScale * 0.1f) * FMath::Max(VehicleScale, 0.01f))) * DeltaTime;
			TrackMaterialInstance->SetScalarParameterValue(TrackOffsetParamName, TrackUVOffset);
		}
	}

	// 2.5 地形跟随（M4b）：贴地 / 按坡面倾斜 / 悬空自由落体。
	// 放在位移之后：这一帧先按上一帧的姿态走，然后探测新位置下的地面并修正。
	// 只在受控实例上跑 —— 远端实例的位置由复制决定，跑了会和复制打架
	if (IsLocallyControlled())
	{
		UpdateGroundContact(DeltaTime);
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

	// 服务器本机的车：NetTurretYaw / NetGunPitch 只有客户端会经 ServerSyncTransform 写入，
	// 主机自己那辆没人写 → 其他端永远看到炮塔朝 0、火炮朝 0（玩家「他人视角又看不到炮塔转」的真因）。
	// 服务器权威 + 本机受控 → 把伺服结果直接写进复制字段，让它随属性复制发出去。
	if (HasAuthority() && IsLocallyControlled())
	{
		NetTurretYaw = CurrentTurretYaw;
		NetGunPitch = CurrentPitch;
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
	// TrackSpan 是网格局部长度，整车缩放后世界半轮距要乘 VehicleScale（M4b）
	const float HalfSpan = FMath::Max(1.0f, TrackSpan) * 0.5f * FMath::Max(VehicleScale, 0.01f);

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
		// RoadWheelRollRadius 是网格局部值；整车缩放后世界滚动半径变小，
		// 同样履带线速度下轮子要转得更快（M4b）
		const float WorldRollRadius = FMath::Max(1.0f, RoadWheelRollRadius * FMath::Max(VehicleScale, 0.01f));
		RoadWheelAngles[i] = FMath::Fmod(
			RoadWheelAngles[i] + SpinSign * FMath::RadiansToDegrees(SideSpeed / WorldRollRadius) * DeltaTime,
			360.0f);
		if (RoadWheels[i])
		{
			RoadWheels[i]->SetRelativeRotation(FRotator(RoadWheelAngles[i], 0.0f, 0.0f));
		}
	}
}

void ATankPawn::ApplyVehicleScale()
{
	// 只缩根组件：车体/履带/负重轮/炮塔/火炮都挂在它下面，等比跟缩。
	// 注意：SpringArm 的 TargetArmLength 是「世界距离」，不会随父级缩放，要单独处理
	if (CollisionBox)
	{
		CollisionBox->SetRelativeScale3D(FVector(FMath::Max(VehicleScale, 0.01f)));
	}
	if (SpringArm)
	{
		SpringArm->TargetArmLength = CameraArmLength * (bScaleCameraWithVehicle ? FMath::Max(VehicleScale, 0.01f) : 1.0f);
	}
}

float ATankPawn::GetBodyHalfHeight() const
{
	// 碰撞盒缩放后的半高（世界单位）——HUD 头顶名牌/血条的锚点高度来源
	return CollisionBox ? CollisionBox->GetScaledBoxExtent().Z : 118.0f * FMath::Max(VehicleScale, 0.01f);
}

// ============================================================================
// 地形跟随（M4b 起，M4c 三轮收口）
// ----------------------------------------------------------------------------
// 算法一句话：采样 → 解目标 → 贴/转 → 安全网。
//   1) 采样：以车心为原点，按 yaw 取水平偏移的 8 个点（四角 + 四边中点），各打一条竖直射线；
//      过滤「竖面 / 高出车角 MaxStepUp 的台面 / 低于车角 MaxContactDrop 的台缘外地面」；
//   2) 解目标：前(0,1,4)/后(2,3,5)/左(0,2,6)/右(1,3,7) 四组，组内先取最小浮空高度当接触候选，
//      只有与它相差 ≤30cm 的采样参与平均（骑棱/压半边坡时另算一块面的点不算数）；
//      前后高差 → pitch（atan2，水平间距 2·ProbeLong）、左右高差 → roll、四点均值 → 车底中心高度；
//      车心目标 Z = 地面高 + 半高 / cos(倾角)；单侧有地时姿态朝水平缓释；
//   3) 贴/转：Z 用 sweep 贴（坠落中把下探量限制在「本帧落体步长 + 5」防一帧瞬移），
//      姿态用根组件 MoveComponent(零位移 + 新旋转, bSweep=true) 扫掠旋转（UE5.8 的 SetActorRotation 无 bSweep 重载）；
//   4) 安全网：角度夹取（pitch ±MaxClimbSlopeDeg / roll ±MaxGroundRollDeg）、坠落沿切面滑落、
//      脱困兜底 DepenetrateIfStuck（内缩 1cm 箱体测阻塞 → 逐级抬起）。
//
// ★★ 升级边界（Chaos 载具迁移 · 阶段 1 取代 / 阶段 3 删除）★★
// 本函数与其状态（VerticalVelocity/bGrounded）在阶段 3 整段删除，故此处不做结构性重构。
// 删除清单与迁移步骤见 docs/tank-vehicle-upgrade-plan.md。
// ============================================================================
void ATankPawn::UpdateGroundContact(float DeltaTime)
{
	if (!CollisionBox)
	{
		return;
	}

	const FVector Extent = CollisionBox->GetScaledBoxExtent();
	const float HalfHeight = FMath::Max(1.0f, Extent.Z);
	const float WalkableCos = FMath::Cos(FMath::DegreesToRadians(MaxClimbSlopeDeg));
	const FVector Loc = GetActorLocation();
	const FRotator CurrentRot = GetActorRotation();

	// ---- 四角地面采样 ----
	// 单点中心采样在坡上必然失明：车身水平爬 30° 坡时坡面在车心正下方已比盒底低
	// 190·tan30° ≈ 110cm（40° 时 159cm），超出 140 窗口 → 探针打空 → 上坡全程 0°、车尾悬空、
	// 下坡到坡底还会翻平拽一帧。改成前/后 × 左/右四点：
	//   前后高差 → pitch，左右高差 → roll，四点均值 → 贴地高度；姿态与高度都由地面说了算。
	// 探针水平位置只按 yaw 旋转：车体俯仰会让轴距在世界里的投影缩短，
	// 但「车头下方是哪块地」必须按固定轴距算，否则坡上前后探针越凑越近、pitch 越算越平
	const FRotationMatrix YawMat(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
	const float ProbeLong = FMath::Max(10.0f, Extent.X - GroundProbeInset);
	const float ProbeLat = FMath::Max(10.0f, Extent.Y - GroundProbeInset);

	// 起点高过任何姿态下的盒顶；终点深到能看见「车角悬空一个轴距」处的坡面 ——
	// 车身水平搭在坡上时，车尾下方的坡面比车尾角点低 2·ProbeLong·tan(坡角)
	const float ProbeUp = Extent.X + HalfHeight + 20.0f;
	const float ProbeDown = 3.0f * Extent.X + HalfHeight + 20.0f;

	FCollisionQueryParams Params(TEXT("TankGroundProbe"), /*bTraceComplex=*/false, this);
	Params.AddIgnoredActor(this);

	// ---- 脱困保险（M4c-3）----
	// 万一车还是嵌进了实体（姿态对齐 / 被推挤 / 出生点重叠），逐级抬起直到「不再阻塞」。
	// 必须存在的原因：一旦埋进去，角点采样会因「地面在角点上方」被排除，
	// 只剩「保持当前姿态」的单侧分支，靠自己永远出不来（M4c-3 实测埋深 160cm 自锁）。
	auto DepenetrateIfStuck = [&]()
	{
		const FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(
			FMath::Max(1.0f, Extent.X - 1.0f),
			FMath::Max(1.0f, Extent.Y - 1.0f),
			FMath::Max(1.0f, Extent.Z - 1.0f)));
		const FVector Base = GetActorLocation();
		const FQuat Rot = GetActorQuat();
		constexpr float LiftStep = 24.0f;
		constexpr int32 MaxSteps = 12;           // 单帧最多抬 288cm（埋得深就分几帧抬出来）
		for (int32 Step = 0; Step <= MaxSteps; ++Step)
		{
			const FVector Test = Base + FVector(0.0f, 0.0f, Step * LiftStep);
			// ECC_Visibility：只认关卡几何（别的坦克对 Visibility 是 Ignore，互相挤压交给推挤链路）
			if (!GetWorld()->OverlapBlockingTestByChannel(Test, Rot, ECC_Visibility, BoxShape, Params))
			{
				if (Step > 0)
				{
					// 目标点已确认无阻塞 → 直接摆放。不要 sweep：起点就在实体里，扫掠会被判 Time=0 原地不动
					SetActorLocation(Test, /*bSweep=*/false);
					VerticalVelocity = 0.0f;
					UE_LOG(LogTank, Log, TEXT("[Terrain] %s 脱困：抬起 %.0fcm"), *GetName(), Step * LiftStep);
				}
				return;
			}
		}
		// 抬满一帧仍阻塞（埋得比 288cm 还深）：先把这一帧能抬的抬满，下一帧接着抬 —— 否则原地不动、永远出不来
		SetActorLocation(Base + FVector(0.0f, 0.0f, MaxSteps * LiftStep), /*bSweep=*/false);
		VerticalVelocity = 0.0f;
		UE_LOG(LogTank, Warning, TEXT("[Terrain] %s 脱困：埋得太深，本帧先抬 %.0fcm（下一帧继续）"),
			*GetName(), MaxSteps * LiftStep);
	};

	// 单个采样点探针。三类命中不算地面：
	//   1) 竖面（掩体立面 / 边界 / 坡的侧面）—— 法线不够「可行走」；
	//   2) 高出该点当前高度 MaxStepUp 以上的台面 —— 那是台阶上沿/别的平台，贴上去就是一帧瞬移；
	//   3) 比该点低 MaxContactDrop 以下的深处地面 —— 那是「台缘外的地面」，不是脚下这块地。
	auto SampleCorner = [&](float AlongX, float AlongY, float& OutZ, float& OutFloat) -> bool
	{
		const FVector Offset = YawMat.TransformVector(FVector(AlongX, AlongY, 0.0f));
		const FVector Start = Loc + Offset + FVector(0.0f, 0.0f, ProbeUp);
		const FVector End = Loc + Offset - FVector(0.0f, 0.0f, ProbeDown);

		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			return false;
		}
		if (Hit.ImpactNormal.Z < WalkableCos)
		{
			return false;
		}
		const float CornerBottom = Loc.Z + CurrentRot.RotateVector(FVector(AlongX, AlongY, -HalfHeight)).Z;
		if (Hit.ImpactPoint.Z > CornerBottom + MaxStepUp)
		{
			return false;
		}
		if (Hit.ImpactPoint.Z < CornerBottom - MaxContactDrop)
		{
			return false;
		}
		OutZ = Hit.ImpactPoint.Z;
		OutFloat = CornerBottom - Hit.ImpactPoint.Z;   // 该点离脚下地面的高度（≈0 = 真正受力）
		return true;
	};

	// 顺序：0=左前 1=右前 2=左后 3=右后 4=前中 5=后中 6=左中 7=右中。
	// 四角 + 四边中点共 8 点（M4c-3「逐点接触」）：骑棱时边中点常是唯一真正压住地面的点，
	// 「对角骑棱、四角全悬空」也不至于丢掉支撑；只在四角采样时，姿态会被棱外那块地的采样带歪。
	const float SampleAlongX[8] = { 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f };
	const float SampleAlongY[8] = { -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 1.0f };
	float SampleZ[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	float SampleFloatHeight[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	bool bSampleOK[8] = { false, false, false, false, false, false, false, false };
	for (int32 i = 0; i < 8; ++i)
	{
		bSampleOK[i] = SampleCorner(SampleAlongX[i] * ProbeLong, SampleAlongY[i] * ProbeLat,
			SampleZ[i], SampleFloatHeight[i]);
	}

	// 组：前(0,1,4) 后(2,3,5) 左(0,2,6) 右(1,3,7)。
	// **姿态只由「真正受力的接触点」决定**：组内先找离车底最近的采样（最小浮空高度 = 接触候选），
	// 比它高出 InlierTol 以上的点不参与平均 —— 那是另一块面，不是这一组脚下的地。
	// 骑棱 / 压半边坡时，正是这些「另一块面」的采样把姿态拉歪的（玩家截图那辆歪车的 roll 来源）。
	auto GroupZ = [&](int32 IndexA, int32 IndexB, int32 IndexC, float& OutZ) -> bool
	{
		float MinFloat = TNumericLimits<float>::Max();
		const int32 Idx[3] = { IndexA, IndexB, IndexC };
		for (int32 k = 0; k < 3; ++k)
		{
			if (bSampleOK[Idx[k]])
			{
				MinFloat = FMath::Min(MinFloat, SampleFloatHeight[Idx[k]]);
			}
		}
		if (MinFloat == TNumericLimits<float>::Max())
		{
			return false;
		}
		const float InlierTol = FMath::Max(MaxStepUp * 2.0f, 30.0f);
		float Sum = 0.0f;
		int32 Count = 0;
		for (int32 k = 0; k < 3; ++k)
		{
			const int32 i = Idx[k];
			if (bSampleOK[i] && SampleFloatHeight[i] <= MinFloat + InlierTol)
			{
				Sum += SampleZ[i];
				++Count;
			}
		}
		if (Count == 0)
		{
			return false;
		}
		OutZ = Sum / (float)Count;
		return true;
	};

	float ZFront = 0.0f;
	float ZRear = 0.0f;
	const bool bHasFront = GroupZ(0, 1, 4, ZFront);
	const bool bHasRear = GroupZ(2, 3, 5, ZRear);

	float TargetPitch = CurrentRot.Pitch;
	float TargetRoll = CurrentRot.Roll;
	float CenterGroundZ = 0.0f;

	if (bHasFront || bHasRear)
	{
		if (bHasFront && bHasRear)
		{
			// 前后探针间距是**水平**的 2·ProbeLong，地面高差 ΔZ → 坡度 tanθ = ΔZ / (2·ProbeLong)。
			// 必须用 atan 而不是 asin：asin(ΔZ/2L) 会随间距变化（30° 坡会算成 asin(tan30°)=35.3°），
			// 姿态一旦超过坡角，车就会以后角为支点翘头、前部整段悬空（M4c 实测坑）
			TargetPitch = FMath::RadiansToDegrees(FMath::Atan2(ZFront - ZRear, 2.0f * ProbeLong));
			CenterGroundZ = (ZFront + ZRear) * 0.5f;
		}
		else
		{
			// 只有一侧有地（另一侧整个悬在台/沟外）：把姿态**朝水平缓释**，而不是死抱当前角度 ——
			// 骑棱悬空冻结时就是死抱 -39° 出不来；缓释几帧后车自己会趴回水平、靠在棱上。
			const float RelaxSpeed = GroundAlignSpeed * 0.25f;
			TargetPitch = FMath::FInterpTo(CurrentRot.Pitch, 0.0f, DeltaTime, RelaxSpeed);
			TargetRoll = FMath::FInterpTo(CurrentRot.Roll, 0.0f, DeltaTime, RelaxSpeed);
			const float SinCur = FMath::Sin(FMath::DegreesToRadians(TargetPitch));
			CenterGroundZ = bHasFront ? ZFront - ProbeLong * SinCur : ZRear + ProbeLong * SinCur;
		}

		float ZLeft = 0.0f;
		float ZRight = 0.0f;
		if (GroupZ(0, 2, 6, ZLeft) && GroupZ(1, 3, 7, ZRight))
		{
			// 左右同理（水平间距 2·ProbeLat）：右高为正 roll（UE 正 roll 抬右舷）
			TargetRoll = FMath::RadiansToDegrees(FMath::Atan2(ZRight - ZLeft, 2.0f * ProbeLat));
		}

		const float PitchRad = FMath::DegreesToRadians(TargetPitch);
		const float RollRad = FMath::DegreesToRadians(TargetRoll);
		const float TiltCos = FMath::Max(0.05f, FMath::Cos(PitchRad) * FMath::Cos(RollRad));
		// 车心到坡面的**竖直**距离 = 半高 / cos(倾角)：车心沿坡面法线离地半高，
		// 法线本身斜了 θ，竖直投影就要除以 cosθ（写成 hh·cosθ 会矮 2·hh·sin²θ 的量，
		// 30° 坡上正好差 18cm —— 车会一直"飘"在坡面上方，M4c 实测抓到的就是这个）
		const float TargetActorZ = CenterGroundZ + HalfHeight / TiltCos;
		const float DeltaZ = TargetActorZ - Loc.Z;

		// 下探窗口：贴地跟随时给足 GroundSnapDownDistance（下坡每帧的下沉量要吃得下）；
		// 真处于坠落中只给「本帧落体步长 + 5」—— 落地那一帧的位移就和前面几帧同量级，
		// 不会出现「一帧贴地 131cm」的瞬移（M4c G3）
		const float MaxDown = (VerticalVelocity < -50.0f)
			? FMath::Max(20.0f, -VerticalVelocity * DeltaTime + 5.0f)
			: GroundSnapDownDistance;

		if (DeltaZ >= -MaxDown && DeltaZ <= MaxStepUp)
		{
			bGrounded = true;
			VerticalVelocity = 0.0f;
			if (!FMath::IsNearlyZero(DeltaZ, 0.25f))
			{
				// 用 sweep 贴地：即使被坡面挡住也只是「停在接触点」，不会穿模
				SetActorLocation(Loc + FVector(0.0f, 0.0f, DeltaZ), /*bSweep=*/true);
			}

			// ---- 按地面姿态倾斜车身：只改 pitch/roll，保留 yaw（转向仍绕车轴）----
			// 不倾斜的话车会以水平姿态插进坡里，视觉与碰撞都难看。
			// **必须带扫掠**：裸的 SetActorRotation 会在骑边缘（一侧悬空）时把车角直接转进坡/掩体里
			// （M4c-3 取证：埋深 160cm、pitch -51.8° 自锁）。UE5.8 的 AActor::SetActorRotation 没有
			// bSweep 重载 → 用根组件的 MoveComponent（零位移 + 新旋转）来扫掠旋转。
			// 角度夹：pitch 夹到最大爬坡角；roll 夹到 MaxGroundRollDeg（本作没有横坡，roll 只可能来自骑棱，
			// 不夹住就会被边缘采样把车带翻 —— 玩家截图那辆歪车）
			const FRotator TargetRot(
				FMath::Clamp(TargetPitch, -MaxClimbSlopeDeg, MaxClimbSlopeDeg),
				CurrentRot.Yaw,
				FMath::Clamp(TargetRoll, -MaxGroundRollDeg, MaxGroundRollDeg));
			CollisionBox->MoveComponent(FVector::ZeroVector,
				FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, GroundAlignSpeed), /*bSweep=*/true);

			DepenetrateIfStuck();
			return;
		}
	}

	// ---- 悬空：走出平台边缘 / 从坡顶开下去 → 自由落体 ----
	// 注意：**刻意不做坠落伤害**。本工程没有任何 FallDamage/Landed 逻辑，
	// TakeDamage 只由炮弹命中调用 —— 落地这里只清速度，绝不要在这里加伤害
	bGrounded = false;
	VerticalVelocity = FMath::Max(VerticalVelocity + GroundGravityZ * DeltaTime, -6000.0f);

	FHitResult FallHit;
	SetActorLocation(Loc + FVector(0.0f, 0.0f, VerticalVelocity * DeltaTime), /*bSweep=*/true, &FallHit);
	if (FallHit.bBlockingHit)
	{
		if (FallHit.ImpactNormal.Z >= WalkableCos)
		{
			// 落到可行走面 → 真落地
			VerticalVelocity = 0.0f;
			bGrounded = true;
		}
		else
		{
			// 蹭到竖面/棱角（法线不可行走）不是支撑：沿接触面切向继续滑落。
			// 否则会「骑在棱上悬空冻结」—— M4c-3 实测：车头搭在掩体边缘棱上、尾浮 223cm、姿态不动；
			// 切向投影对竖面是整段落体（沿墙滑下），对棱角是「往外挪一点」，最终自己滑下去。
			const FVector SlideStep = FVector::VectorPlaneProject(
				FVector(0.0f, 0.0f, VerticalVelocity * DeltaTime), FallHit.ImpactNormal);
			if (!SlideStep.IsNearlyZero())
			{
				// 沿切向**直接摆放**：此时车正贴着棱/墙（扫掠起点就已接触），带扫掠的位移会被判 Time=0 原地不动
				SetActorLocation(GetActorLocation() + SlideStep, /*bSweep=*/false);
			}
		}
	}

	DepenetrateIfStuck();
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
