#pragma once

#include <atomic>

#include "SWDepthBuffer.h"

#include "../../DataType/Math/AABB.h"

#include "../CullingModule.h"

#include "Stage/BinTrianglesStage.h"
#include "Stage/RasterizeTrianglesStage.h"


namespace culling
{
	/// <summary>
	/// 
	/// Masked SW Occlusion Culling
	/// 
	/// 
	/// Supported SIMD Version : AVX2
	/// 
	/// How Wokrs ? : 
	/// Please read "MaskedSWOcclusionCulling_HowWorks.md" file
	/// 
	/// references : 
	/// https://www.slideshare.net/IntelSoftware/masked-software-occlusion-culling
	/// https://www.slideshare.net/IntelSoftware/masked-occlusion-culling
	/// </summary>
	class MaskedSWOcclusionCulling : public CullingModule
	{
		friend class EveryCulling;
		friend class BinTrianglesStage;
		friend class MaskedSWOcclusionCullingStage;

		enum class eMaskedOcclusionCullingStage
		{
			BinTriangles,
			DrawOccluder,

		};

	private:

		BinTrianglesStage mBinTrianglesStage;
		RasterizeTrianglesStage mRasterizeTrianglesStage;

		SWDepthBuffer mDepthBuffer;
		
		const std::uint32_t binCountInRow, binCountInColumn;
		float mNearClipPlaneDis, mFarClipPlaneDis;

		void ResetDepthBuffer();




		/// <summary>
		/// 
		/// </summary>
		/// <param name="vertices"></param>
		/// <param name="vertexIndices"></param>
		/// <param name="indiceCount"></param>
		/// <param name="vertexStrideByte">
		/// how far next vertex point is from current vertex point 
		/// ex) 
		/// 1.0f(Point1_X), 2.0f(Point2_Y), 0.0f(Point3_Z), 3.0f(Normal_X), 3.0f(Normal_Y), 3.0f(Normal_Z),  1.0f(Point1_X), 2.0f(Point2_Y), 0.0f(Point3_Z)
		/// --> vertexStride is 6 * 4(float)
		/// </param>
		/// <param name="modelToClipspaceMatrix"></param>
		void DrawOccluderTriangles
		(
			const float* vertices, 
			const std::uint32_t* vertexIndices, 
			size_t indiceCount, 
			bool vertexStrideByte,
			float* modelToClipspaceMatrix
		);


		/// <summary>
		/// Depth Test Multiple Occludees
		/// </summary>
		/// <param name="worldAABBs"></param>
		void DepthTestOccludee(const AABB* worldAABBs, size_t aabbCount, char* visibleBitFlags);

		// Inherited via CullingModule
		virtual void CullBlockEntityJob(EntityBlock* currentEntityBlock, size_t entityCountInBlock, size_t cameraIndex) override;

		void GetOccluderCandidates()
		{

		}

	public:

		MaskedSWOcclusionCulling
		(
			EveryCulling* everyCulling,
			const std::uint32_t depthBufferWidth, 
			const std::uint32_t depthBufferheight
		);
	
		void SetNearFarClipPlaneDistance(const float nearClipPlaneDis, const float farClipPlaneDis);

		void ResetState();
		
		

		void MaskedSWOcclusionJob();
	};
}

