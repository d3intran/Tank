#pragma once

#include "CoreMinimal.h"

namespace TankVehicleLayout
{
	// ---- 布局常量（网格空间 cm，与 build_tank_skeleton.py 严格一致）----
	constexpr float RoadWheelY = 144.0f;           // 轮心左右偏移
	constexpr float RoadWheelZ = 41.75f;           // 轮心高度（履带底面之上）
	constexpr float TrackSpan = 278.4f;            // 左右履带中心距（差速/UV 用，网格空间）
	constexpr int32 WheelsPerSide = 6;

	// 碰撞盒：与旧 CollisionBox 逐值一致（UBoxComponent 的 extent 是半尺寸；这里换成全尺寸填 FKBoxElem）
	constexpr float ChassisBoxHalfX = 380.0f;
	constexpr float ChassisBoxHalfY = 175.0f;
	constexpr float ChassisBoxHalfZ = 118.0f;
	constexpr float ChassisBoxCenterZ = 120.0f;

	const TCHAR* const SkeletalMeshPath = TEXT("/Game/tank/ztz-88a/ztz88a_skeletal.ztz88a_skeletal");
	const TCHAR* const TurretMeshPath = TEXT("/Game/tank/ztz-88a/ztz88a-turret.ztz88a-turret");
	const TCHAR* const GunMeshPath = TEXT("/Game/tank/ztz-88a/ztz88a-gun.ztz88a-gun");

	/** 负重轮静态网格 + 网格空间 X。顺序：0-5 右(+Y)，6-11 左(-Y) */
	struct FRoadWheelVisual
	{
		const TCHAR* MeshPath;
		float X;
		bool bRightSide;
	};

	constexpr FRoadWheelVisual RoadSetups[] =
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
}
