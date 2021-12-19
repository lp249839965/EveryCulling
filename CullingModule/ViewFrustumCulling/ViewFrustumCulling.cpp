#include "ViewFrustumCulling.h"

#include <cassert>

#include "../../DataType/Math/Common.h"
#include "../../EveryCulling.h"

#include <Rendering/Renderer/Renderer.h>
#include <Transform.h>


culling::ViewFrustumCulling::ViewFrustumCulling(EveryCulling* frotbiteCullingSystem)
	: CullingModule{ frotbiteCullingSystem }
{

}

void culling::ViewFrustumCulling::CullBlockEntityJob
(
	EntityBlock* const currentEntityBlock,
	const size_t entityCountInBlock,
	const size_t cameraIndex
)
{


	const math::Vector3* const cameraPos = reinterpret_cast<const math::Vector3*>(&(mCullingSystem->GetCameraPosition(cameraIndex)));

	for (size_t entityIndex = 0; entityIndex < entityCountInBlock; entityIndex++)
	{
		dooms::Transform* const transform = reinterpret_cast<dooms::Transform*>(currentEntityBlock->mTransform[entityIndex]);
		dooms::Renderer* const renderer = reinterpret_cast<dooms::Renderer*>(currentEntityBlock->mRenderer[entityIndex]);

		if(dooms::IsValid(renderer) == true)
		{
			Position_BoundingSphereRadius* const posBoundingSphereRadius = currentEntityBlock->mPositionAndBoundingSpheres + entityIndex;

			const float worldRadius = renderer->dooms::ColliderUpdater<dooms::physics::Sphere>::GetWorldCollider()->mRadius;

			const culling::Vec3* const entityPos = reinterpret_cast<const culling::Vec3*>(&transform->GetPosition());
			*reinterpret_cast<culling::M128F*>(posBoundingSphereRadius) = *reinterpret_cast<const culling::M128F*>(entityPos);
			posBoundingSphereRadius->SetBoundingSphereRadius(worldRadius);
		}		
	}

	// this object's size should be multiples of 32
	const size_t cullingMaskSize = ENTITY_COUNT_IN_ENTITY_BLOCK + 32 - (ENTITY_COUNT_IN_ENTITY_BLOCK % 32);
	alignas(32) char cullingMask[cullingMaskSize];
	D_ASSERT(cullingMaskSize % 32 == 0);
	for(int i = 0 ; i < cullingMaskSize ; i += 32)
	{
		*reinterpret_cast<culling::M256F*>((char*)cullingMask + i) = _mm256_setzero_ps();
	}

	const Vec4* frustumPlane = mSIMDFrustumPlanes[cameraIndex].mFrustumPlanes;

	for (size_t entityIndex = 0; entityIndex < entityCountInBlock - 1 ; entityIndex = entityIndex + 2)
	{
		char result = CheckInFrustumSIMDWithTwoPoint(frustumPlane, currentEntityBlock->mPositionAndBoundingSpheres + entityIndex);
		// if first low bit has 1 value, Pos A is In Frustum
		// if second low bit has 1 value, Pos A is In Frustum

		//for maximizing cache hit, Don't set Entity's IsVisiable at here
		cullingMask[entityIndex] |= (result | ~1) << cameraIndex;
		cullingMask[entityIndex + 1] |= ((result | ~2) >> 1) << cameraIndex;

	}

	//TODO : If CullingMask is True, Do Calculate ScreenSpace AABB Area And Check Is Culled
	// use mCulledScreenSpaceAABBArea
	culling::M256F* m256f_isVisible = reinterpret_cast<culling::M256F*>(currentEntityBlock->mIsVisibleBitflag);
	const culling::M256F* m256f_cullingMask = reinterpret_cast<const culling::M256F*>(cullingMask);
	const std::uint32_t m256_count_isvisible = cullingMaskSize / 32;

	/// <summary>
	/// M256 = 8bit(1byte = bool size) * 32 
	/// 
	/// And operation with result culling mask and entityblock's visible bitflag
	/// </summary>
	for (std::uint32_t i = 0; i < m256_count_isvisible; i++)
	{
		m256f_isVisible[i] = _mm256_and_ps(m256f_isVisible[i], m256f_cullingMask[i]);
	}
}

FORCE_INLINE char culling::ViewFrustumCulling::CheckInFrustumSIMDWithTwoPoint(const Vec4* eightPlanes,
	const Position_BoundingSphereRadius* twoPoint)
{
	//We can't use culling::M256F. because two twoPoint isn't aligned to 32 byte

	const culling::M128F* const m128f_eightPlanes = reinterpret_cast<const culling::M128F*>(eightPlanes); // x of plane 0, 1, 2, 3  and y of plane 0, 1, 2, 3 
	const culling::M128F* const m128f_2Point = reinterpret_cast<const culling::M128F*>(twoPoint);

	const culling::M128F posA_xxxx = M128F_REPLICATE(m128f_2Point[0], 0); // xxxx of first twoPoint and xxxx of second twoPoint
	const culling::M128F posA_yyyy = M128F_REPLICATE(m128f_2Point[0], 1); // yyyy of first twoPoint and yyyy of second twoPoint
	const culling::M128F posA_zzzz = M128F_REPLICATE(m128f_2Point[0], 2); // zzzz of first twoPoint and zzzz of second twoPoint

	const culling::M128F posA_rrrr = M128F_REPLICATE(m128f_2Point[0], 3); // rrrr of first twoPoint and rrrr of second twoPoint

	culling::M128F dotPosA = culling::M128F_MUL_AND_ADD(posA_zzzz, m128f_eightPlanes[2], m128f_eightPlanes[3]);
	dotPosA = culling::M128F_MUL_AND_ADD(posA_yyyy, m128f_eightPlanes[1], dotPosA);
	dotPosA = culling::M128F_MUL_AND_ADD(posA_xxxx, m128f_eightPlanes[0], dotPosA); // dot Pos A with Plane 0, dot Pos A with Plane 1, dot Pos A with Plane 2, dot Pos A with Plane 3

	const culling::M128F posB_xxxx = M128F_REPLICATE(m128f_2Point[1], 0); // xxxx of first twoPoint and xxxx of second twoPoint
	const culling::M128F posB_yyyy = M128F_REPLICATE(m128f_2Point[1], 1); // yyyy of first twoPoint and yyyy of second twoPoint
	const culling::M128F posB_zzzz = M128F_REPLICATE(m128f_2Point[1], 2); // zzzz of first twoPoint and zzzz of second twoPoint

	const culling::M128F posB_rrrr = M128F_REPLICATE(m128f_2Point[1], 3); // rrrr of first twoPoint and rrrr of second twoPoint

	culling::M128F dotPosB = culling::M128F_MUL_AND_ADD(posB_zzzz, m128f_eightPlanes[2], m128f_eightPlanes[3]);
	dotPosB = culling::M128F_MUL_AND_ADD(posB_yyyy, m128f_eightPlanes[1], dotPosB);
	dotPosB = culling::M128F_MUL_AND_ADD(posB_xxxx, m128f_eightPlanes[0], dotPosB); // dot Pos B with Plane 0, dot Pos B with Plane 1, dot Pos B with Plane 2, dot Pos B with Plane 3

	//https://software.intel.com/sites/landingpage/IntrinsicsGuide/#expand=69,124,4167,4167,447,447,3148,3148&techs=SSE,SSE2,SSE3,SSSE3,SSE4_1,SSE4_2,AVX&text=insert
	const culling::M128F posAB_xxxx = _mm_shuffle_ps(m128f_2Point[0], m128f_2Point[1], SHUFFLEMASK(0, 0, 0, 0)); // x of twoPoint[0] , x of twoPoint[0], x of twoPoint[1] , x of twoPoint[1]
	const culling::M128F posAB_yyyy = _mm_shuffle_ps(m128f_2Point[0], m128f_2Point[1], SHUFFLEMASK(1, 1, 1, 1)); // y of twoPoint[0] , y of twoPoint[0], y of twoPoint[1] , y of twoPoint[1]
	const culling::M128F posAB_zzzz = _mm_shuffle_ps(m128f_2Point[0], m128f_2Point[1], SHUFFLEMASK(2, 2, 2, 2)); // z of twoPoint[0] , z of twoPoint[0], z of twoPoint[1] , z of twoPoint[1]

	const culling::M128F posAB_rrrr = _mm_shuffle_ps(m128f_2Point[0], m128f_2Point[1], SHUFFLEMASK(3, 3, 3, 3)); // r of twoPoint[0] , r of twoPoint[1], w of twoPoint[1] , w of twoPoint[1]

	culling::M128F dotPosAB45 = culling::M128F_MUL_AND_ADD(posAB_zzzz, m128f_eightPlanes[6], m128f_eightPlanes[7]);
	dotPosAB45 = culling::M128F_MUL_AND_ADD(posAB_yyyy, m128f_eightPlanes[5], dotPosAB45);
	dotPosAB45 = culling::M128F_MUL_AND_ADD(posAB_xxxx, m128f_eightPlanes[4], dotPosAB45);

	dotPosA = _mm_cmpgt_ps(dotPosA, posA_rrrr); // if elemenet[i] have value 1, Pos A is in frustum Plane[i] ( 0 <= i < 4 )
	dotPosB = _mm_cmpgt_ps(dotPosB, posB_rrrr); // if elemenet[i] have value 1, Pos B is in frustum Plane[i] ( 0 <= i < 4 )
	dotPosAB45 = _mm_cmpgt_ps(dotPosAB45, posAB_rrrr);

	// this is wrong
	//dotPosA = _mm_cmpgt_ps(posA_rrrr, dotPosA); // if elemenet[i] have value 1, Pos A is in frustum Plane[i] ( 0 <= i < 4 )
	//dotPosB = _mm_cmpgt_ps(posB_rrrr, dotPosB); // if elemenet[i] have value 1, Pos B is in frustum Plane[i] ( 0 <= i < 4 )
	//dotPosAB45 = _mm_cmpgt_ps(posAB_rrrr, dotPosAB45);

	const culling::M128F dotPosA45 = _mm_blend_ps(dotPosAB45, dotPosA, SHUFFLEMASK(0, 3, 0, 0)); // Is In Plane with Plane[4], Plane[5], Plane[2], Plane[3]
	const culling::M128F dotPosB45 = _mm_blend_ps(dotPosB, dotPosAB45, SHUFFLEMASK(0, 3, 0, 0)); // Is In Plane with Plane[0], Plane[1], Plane[4], Plane[5]

	culling::M128F RMaskA = _mm_and_ps(dotPosA, dotPosA45); //when everty bits is 1, PointA is in frustum
	culling::M128F RMaskB = _mm_and_ps(dotPosB, dotPosB45);//when everty bits is 1, PointB is in frustum

	INT32 IsPointAInFrustum = _mm_test_all_ones(*reinterpret_cast<M128I*>(&RMaskA)); // value is 1, Point in in frustum
	INT32 IsPointBInFrustum = _mm_test_all_ones(*reinterpret_cast<M128I*>(&RMaskB));

	char IsPointABInFrustum = IsPointAInFrustum | (IsPointBInFrustum << 1);
	return IsPointABInFrustum;
}

void culling::ViewFrustumCulling::SetViewProjectionMatrix(const size_t cameraIndex, const Mat4x4& viewProjectionMatrix)
{
	culling::CullingModule::SetViewProjectionMatrix(cameraIndex, viewProjectionMatrix);

	assert(cameraIndex >= 0 && cameraIndex < MAX_CAMERA_COUNT);

	ExtractSIMDPlanesFromViewProjectionMatrix(viewProjectionMatrix, mSIMDFrustumPlanes[cameraIndex].mFrustumPlanes, true);
}

