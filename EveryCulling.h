#pragma once

#include "EveryCullingCore.h"
#include "DataType/EntityGridCell.h"
#include "DataType/EntityBlockViewer.h"

#include <array>
#include <vector>
#include <atomic>
#include <functional>
#include <thread>

#include "CullingModule/ViewFrustumCulling/ViewFrustumCulling.h"

#ifdef ENABLE_SCREEN_SAPCE_BOUDING_SPHERE_CULLING
#include "CullingModule/ScreenSpaceBoundingSphereCulling/ScreenSpaceBoundingSphereCulling.h"
#endif

#include "CullingModule/MaskedSWOcclusionCulling/MaskedSWOcclusionCulling.h"

#ifdef ENABLE_QUERY_OCCLUSION
#include "CullingModule/QueryOcclusionCulling/QueryOcclusionCulling.h"
#endif

namespace culling
{
	/// <summary>
	/// 
	/// This is implementation of Data Oriented ViewFrustumCulling of Frostbite in 2011
	///
	/// [Slide Resource](https://www.ea.com/frostbite/news/culling-the-battlefield-data-oriented-design-in-practice)        
	/// [GDC Talk Video](https://www.gdcvault.com/play/1014491/Culling-the-Battlefield-Data-Oriented)          
	/// [�ѱ��� ���α� ��] (https ://sungjjinkang.github.io/doom/2021/04/02/viewfrustumculling.html)  
	///
	/// 
	/// This culling use SIMD DotProduct, So Check LightMath_Cpp/Mat4x4Float_Aligned.inl
	///
	/// Feature :
	/// 1. Linear arrays scale great. All entity's position data is stored linearlly.
	/// 2. Tree based sturucture or other Acceleration strucutures isn;t required 
	/// 3. Frustum vs Sphere intersections check per loop
	/// 3. Threads solve intersections(blocks) parrallily
	/// 
	/// this library require math library : https://github.com/SungJJinKang/LightMath_Cpp/blob/main/Mat4x4Float_Aligned.inl
	/// 
	/// references : 
	/// https://www.gdcvault.com/play/1014491/Culling-the-Battlefield-Data-Oriented
	/// https://www.slideshare.net/DICEStudio/culling-the-battlefield-data-oriented-design-in-practice
	/// https://macton.smugmug.com/Other/2008-07-15-by-Eye-Fi/n-xmKDH/
	/// 
	/// </summary>
	class DOOM_API EveryCulling
	{
		friend class ScreenSpaceBoundingSphereCulling;
		friend class ViewFrustumCulling;
		friend class QueryOcclusionCulling;

	private:

		UINT32 mCameraCount;
		std::array<culling::Vec3, MAX_CAMERA_COUNT> mCameraPositions;

		bool bmIsEntityBlockPoolInitialized{ false };

		/// <summary>
		/// List of EntityBlock with no Entity ( maybe entity was destroyed)
		/// </summary>
		std::vector<EntityBlock*> mFreeEntityBlockList{};
		/// <summary>
		/// List of EntityBlock with Entities
		/// </summary>
		std::vector<EntityBlock*> mActiveEntityBlockList{};
		/// <summary>
		/// Allocated EntityBlock Arrays
		/// This objects will be released at destructor
		/// </summary>
		std::vector<EntityBlock*> mAllocatedEntityBlockChunkList{};
		EntityGridCell mEntityGridCell{};

		std::atomic<bool> mIsCullJobFinished;

		void AllocateEntityBlockPool();
		void ResetEntityBlock(culling::EntityBlock* entityBlock);
		std::pair<culling::EntityBlock*, UINT32*> AllocateNewEntityBlockFromPool();
		void RemoveEntityFromBlock(EntityBlock* ownerEntityBlock, UINT32 entityIndexInBlock);
		/// <summary>
		/// Block Swap removedblock with last block, and return swapped lastblock to pool
		/// </summary>
		void FreeEntityBlock(EntityBlock* freedEntityBlock);
		EntityBlock* GetNewEntityBlockFromPool();

		void ResetCullJobStateVariable();
		/// <summary>
		/// Reset VisibleFlag
		/// </summary>
		void SetAllOneIsVisibleFlag();

	public:

		ViewFrustumCulling mViewFrustumCulling;
#ifdef ENABLE_SCREEN_SAPCE_BOUDING_SPHERE_CULLING
		ScreenSpaceBoundingSphereCulling mScreenSpaceBoudingSphereCulling;
#endif
		MaskedSWOcclusionCulling mMaskedSWOcclusionCulling;
#ifdef ENABLE_QUERY_OCCLUSION
		QueryOcclusionCulling mQueryOcclusionCulling;
#endif

	private:

		std::array<culling::CullingModule*, 
#ifdef ENABLE_SCREEN_SAPCE_BOUDING_SPHERE_CULLING
			3
#else
			1
#endif
		> mUpdatedCullingModules
		{
			&(mViewFrustumCulling),
#ifdef ENABLE_SCREEN_SAPCE_BOUDING_SPHERE_CULLING
			&(mScreenSpaceBoudingSphereCulling),
#endif	
			//&(mMaskedSWOcclusionCulling)
		};

	public:

		EveryCulling(UINT32 resolutionWidth, UINT32 resolutionHeight);
		~EveryCulling();
		void SetCameraCount(UINT32 cameraCount);
		void SetCameraPosition(const SIZE_T cameraIndex, const culling::Vec3& cameraPosition);

		void SetViewProjectionMatrix(const UINT32 cameraIndex, const culling::Mat4x4& viewProjectionMatrix);
		UINT32 GetCameraCount() const;
		inline const culling::Vec3& GetCameraPosition(const SIZE_T cameraIndex) const
		{
			assert(cameraIndex >= 0 && cameraIndex < MAX_CAMERA_COUNT);
			return mCameraPositions[cameraIndex];
		}

		/// <summary>
		/// Get EntityBlock List with entities
		/// </summary>
		/// <returns></returns>
		const std::vector<EntityBlock*>& GetActiveEntityBlockList() const;

		/// <summary>
		/// You should call this function on your Transform Component or In your game engine
		///
		/// increment EntityCountInBlock of TargetBlock.
		/// If All blocks is full, Get new block from Block Pool
		/// 
		/// Allocating New Entity isn't thread safe
		/// </summary>
		EntityBlockViewer AllocateNewEntity(void* renderer, void* transform);

		/// <summary>
		/// You should call this function on your Transform Component or In your game engine
		/// 
		/// Remove Entity is nothing, Just decrement AllocatedEntityCountInBlocks
		/// And if AllocatedEntityCountInBlocks Of Block become zero, Remove the block using RemoveBlock function
		/// 
		/// Removing Entity isn't thread safe
		/// </summary>
		void RemoveEntityFromBlock(EntityBlockViewer& entityBlockViewer);

		/// <summary>
		/// Solve View Frustum Culling from multiple threads
		/// Call this before start draw
		/// 
		/// for pushing to job pool , this function is declared with static
		/// 
		/// CullBlockEntityJob never access to shared variable.
		/// So CullBlockEntityJob is thread safe.
		/// </summary>
		void CullBlockEntityJob();
		void CullBlockEntityJob(const SIZE_T cameraIndex);
		//void CullBlockEntityJob(const UINT32 threadIndex, const UINT32 threadCount);

		/// <summary>
		/// Get Is All block's culling job is finished.
		/// if true, you can check object's visibility(culled)
		/// </summary>
		/// <returns></returns>
		bool GetIsCullJobFinished(const std::atomic<UINT32>& mFinishedCullEntityBlockCount, UINT32 entityBlockCount) const;

		/// <summary>
		/// Caller thread will stall until cull job of all entity block is finished
		/// </summary>
		void WaitToFinishCullJob(const UINT32 cameraIndex) const;
		void WaitToFinishCullJobOfAllCameras() const;

		/// <summary>
		/// Reset cull job state before pushing cull jobs to job pool
		/// </summary>
		void ResetCullJobState();

		/// <summary>
		/// Return Cached cull job std::function 
		/// </summary>
		/// <returns></returns>
		constexpr auto GetCullJobInLambda()
		{
			return [this]() 
			{
				this->CullBlockEntityJob();
			};
		}

		constexpr auto GetCullJobInLambda(const SIZE_T cameraIndex)
		{
			return [this, cameraIndex]()
			{
				this->CullBlockEntityJob(cameraIndex);
			};
		}
	};
}


