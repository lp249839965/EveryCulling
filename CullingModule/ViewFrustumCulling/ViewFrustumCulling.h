#pragma once

#include <vector>
#include <functional>

#include "../../DataType/Math/Matrix.h"
#include "../../DataType/Math/Vector.h"

#include "../CullingModule.h"

#include "../../SIMD_Core.h"

namespace culling
{
	struct alignas(64) SIMDFrustumPlanes // for cache hit, align to 64 byte
	{
		Vector4 mFrustumPlanes[8]; // first 4 planes will be on same cache line
	};

	class FrotbiteCullingSystem;
	class ViewFrustumCulling : CullingModule
	{
		friend class FrotbiteCullingSystem;

	private:

		SIMDFrustumPlanes mSIMDFrustumPlanes[MAX_CAMERA_COUNT];

		ViewFrustumCulling(FrotbiteCullingSystem* frotbiteCullingSystem)
			:CullingModule{ frotbiteCullingSystem }
		{

		}

		void CullBlockEntityJob(EntityBlock* currentEntityBlock, size_t entityCountInBlock, size_t cameraIndex)
		{
			alignas(64) char cullingMask[ENTITY_COUNT_IN_ENTITY_BLOCK] = { 0 };

			const Vector4* frustumPlane = this->mSIMDFrustumPlanes[cameraIndex].mFrustumPlanes;
			for (size_t j = 0; j < entityCountInBlock; j = j + 2)
			{
				char result = this->CheckInFrustumSIMDWithTwoPoint(frustumPlane, currentEntityBlock->mPositions + j);
				// if first low bit has 1 value, Pos A is In Frustum
				// if second low bit has 1 value, Pos A is In Frustum

				//for maximizing cache hit, Don't set Entity's IsVisiable at here
				cullingMask[j] |= (result | ~1) << cameraIndex;
				cullingMask[j + 1] |= ((result | ~2) >> 1) << cameraIndex;

			}

			//TODO : If CullingMask is True, Do Calculate ScreenSpace AABB Area And Check Is Culled
			// use mCulledScreenSpaceAABBArea
			M256F* m256f_isVisible = reinterpret_cast<M256F*>(currentEntityBlock->mIsVisibleBitflag);
			const M256F* m256f_cullingMask = reinterpret_cast<const M256F*>(cullingMask);
			const unsigned int m256_count_isvisible = 1 + ((currentEntityBlock->mCurrentEntityCount * sizeof(decltype(*EntityBlock::mIsVisibleBitflag)) - 1) / sizeof(M256F));

			/// <summary>
			/// M256 = 8bit(1byte = bool size) * 32 
			/// 
			/// And operation with result culling mask and entityblock's visible bitflag
			/// </summary>
			for (unsigned int i = 0; i < m256_count_isvisible; i++)
			{
				m256f_isVisible[i] = _mm256_and_ps(m256f_isVisible[i], m256f_cullingMask[i]);
			}

		}
		FORCE_INLINE char CheckInFrustumSIMDWithTwoPoint(const Vector4* eightPlanes, const Vector4* twoPoint)
		{
			//We can't use M256F. because two twoPoint isn't aligned to 32 byte

			const M128F* m128f_eightPlanes = reinterpret_cast<const M128F*>(eightPlanes); // x of plane 0, 1, 2, 3  and y of plane 0, 1, 2, 3 
			const M128F* m128f_2Point = reinterpret_cast<const M128F*>(twoPoint);

			const M128F posA_xxxx = M128F_REPLICATE(m128f_2Point[0], 0); // xxxx of first twoPoint and xxxx of second twoPoint
			const M128F posA_yyyy = M128F_REPLICATE(m128f_2Point[0], 1); // yyyy of first twoPoint and yyyy of second twoPoint
			const M128F posA_zzzz = M128F_REPLICATE(m128f_2Point[0], 2); // zzzz of first twoPoint and zzzz of second twoPoint

			const M128F posA_rrrr = M128F_REPLICATE(m128f_2Point[0], 3); // rrrr of first twoPoint and rrrr of second twoPoint

			M128F dotPosA = culling::M128F_MUL_AND_ADD(posA_zzzz, m128f_eightPlanes[2], m128f_eightPlanes[3]);
			dotPosA = culling::M128F_MUL_AND_ADD(posA_yyyy, m128f_eightPlanes[1], dotPosA);
			dotPosA = culling::M128F_MUL_AND_ADD(posA_xxxx, m128f_eightPlanes[0], dotPosA); // dot Pos A with Plane 0, dot Pos A with Plane 1, dot Pos A with Plane 2, dot Pos A with Plane 3

			const M128F posB_xxxx = M128F_REPLICATE(m128f_2Point[1], 0); // xxxx of first twoPoint and xxxx of second twoPoint
			const M128F posB_yyyy = M128F_REPLICATE(m128f_2Point[1], 1); // yyyy of first twoPoint and yyyy of second twoPoint
			const M128F posB_zzzz = M128F_REPLICATE(m128f_2Point[1], 2); // zzzz of first twoPoint and zzzz of second twoPoint

			const M128F posB_rrrr = M128F_REPLICATE(m128f_2Point[1], 3); // rrrr of first twoPoint and rrrr of second twoPoint

			M128F dotPosB = culling::M128F_MUL_AND_ADD(posB_zzzz, m128f_eightPlanes[2], m128f_eightPlanes[3]);
			dotPosB = culling::M128F_MUL_AND_ADD(posB_yyyy, m128f_eightPlanes[1], dotPosB);
			dotPosB = culling::M128F_MUL_AND_ADD(posB_xxxx, m128f_eightPlanes[0], dotPosB); // dot Pos B with Plane 0, dot Pos B with Plane 1, dot Pos B with Plane 2, dot Pos B with Plane 3

																				   //https://software.intel.com/sites/landingpage/IntrinsicsGuide/#expand=69,124,4167,4167,447,447,3148,3148&techs=SSE,SSE2,SSE3,SSSE3,SSE4_1,SSE4_2,AVX&text=insert
			const M128F posAB_xxxx = _mm_shuffle_ps(m128f_2Point[0], m128f_2Point[1], SHUFFLEMASK(0, 0, 0, 0)); // x of twoPoint[0] , x of twoPoint[0], x of twoPoint[1] , x of twoPoint[1]
			const M128F posAB_yyyy = _mm_shuffle_ps(m128f_2Point[0], m128f_2Point[1], SHUFFLEMASK(1, 1, 1, 1)); // y of twoPoint[0] , y of twoPoint[0], y of twoPoint[1] , y of twoPoint[1]
			const M128F posAB_zzzz = _mm_shuffle_ps(m128f_2Point[0], m128f_2Point[1], SHUFFLEMASK(2, 2, 2, 2)); // z of twoPoint[0] , z of twoPoint[0], z of twoPoint[1] , z of twoPoint[1]

			const M128F posAB_rrrr = _mm_shuffle_ps(m128f_2Point[0], m128f_2Point[1], SHUFFLEMASK(3, 3, 3, 3)); // r of twoPoint[0] , r of twoPoint[1], w of twoPoint[1] , w of twoPoint[1]

			M128F dotPosAB45 = culling::M128F_MUL_AND_ADD(posAB_zzzz, m128f_eightPlanes[6], m128f_eightPlanes[7]);
			dotPosAB45 = culling::M128F_MUL_AND_ADD(posAB_yyyy, m128f_eightPlanes[5], dotPosAB45);
			dotPosAB45 = culling::M128F_MUL_AND_ADD(posAB_xxxx, m128f_eightPlanes[4], dotPosAB45);

			dotPosA = _mm_cmpgt_ps(dotPosA, posA_rrrr); // if elemenet[i] have value 1, Pos A is in frustum Plane[i] ( 0 <= i < 4 )
			dotPosB = _mm_cmpgt_ps(dotPosB, posB_rrrr); // if elemenet[i] have value 1, Pos B is in frustum Plane[i] ( 0 <= i < 4 )
			dotPosAB45 = _mm_cmpgt_ps(dotPosAB45, posAB_rrrr);

			// this is wrong
			//dotPosA = _mm_cmpgt_ps(posA_rrrr, dotPosA); // if elemenet[i] have value 1, Pos A is in frustum Plane[i] ( 0 <= i < 4 )
			//dotPosB = _mm_cmpgt_ps(posB_rrrr, dotPosB); // if elemenet[i] have value 1, Pos B is in frustum Plane[i] ( 0 <= i < 4 )
			//dotPosAB45 = _mm_cmpgt_ps(posAB_rrrr, dotPosAB45);

			const M128F dotPosA45 = _mm_blend_ps(dotPosAB45, dotPosA, SHUFFLEMASK(0, 3, 0, 0)); // Is In Plane with Plane[4], Plane[5], Plane[2], Plane[3]
			const M128F dotPosB45 = _mm_blend_ps(dotPosB, dotPosAB45, SHUFFLEMASK(0, 3, 0, 0)); // Is In Plane with Plane[0], Plane[1], Plane[4], Plane[5]

			M128F RMaskA = _mm_and_ps(dotPosA, dotPosA45); //when everty bits is 1, PointA is in frustum
			M128F RMaskB = _mm_and_ps(dotPosB, dotPosB45);//when everty bits is 1, PointB is in frustum

			int IsPointAInFrustum = _mm_test_all_ones(*reinterpret_cast<M128I*>(&RMaskA)); // value is 1, Point in in frustum
			int IsPointBInFrustum = _mm_test_all_ones(*reinterpret_cast<M128I*>(&RMaskB));

			char IsPointABInFrustum = IsPointAInFrustum | (IsPointBInFrustum << 1);
			return IsPointABInFrustum;
		}

	public:
		/// <summary>
		/// before Start solving culling, Update Every Camera's frustum plane
		/// Do this at main thread
		/// </summary>
		/// <param name="frustumPlaneIndex"></param>
		/// <param name="viewProjectionMatix"></param>
		void UpdateFrustumPlane(unsigned int frustumPlaneIndex, const Matrix4X4& viewProjectionMatrix);
		FORCE_INLINE culling::SIMDFrustumPlanes* GetSIMDPlanes()
		{
			return this->mSIMDFrustumPlanes;
		}
	};
}
