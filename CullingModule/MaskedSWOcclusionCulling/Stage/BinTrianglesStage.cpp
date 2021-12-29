#include "BinTrianglesStage.h"

#include <vector>

#include "../MaskedSWOcclusionCulling.h"
#include "../SWDepthBuffer.h"
#include "../Utility/vertexTransformationHelper.h"
#include "../Utility/depthBufferTileHelper.h"

#include <Rendering/Renderer/Renderer.h>
#include <Rendering/Renderer/RendererStaticIterator.h>

#include "../Utility/RasterizerHelper.h"


#define CONVERT_TO_M256I(_M256F) *reinterpret_cast<const culling::M256I*>(&_M256F)

FORCE_INLINE void culling::BinTrianglesStage::Clipping
(
	const culling::M256F* const clipspaceVertexX,
	const culling::M256F* const clipspaceVertexY,
	const culling::M256F* const clipspaceVertexZ,
	const culling::M256F* const clipspaceVertexW,
	std::uint32_t& triangleCullMask
)
{
	const culling::M256F pointANdcW = clipspaceVertexW[0];
	const culling::M256F pointANdcNegativeW = _mm256_xor_ps(_mm256_set1_ps(-0.0f), clipspaceVertexW[0]);
	const culling::M256F pointBNdcW = clipspaceVertexW[1];
	const culling::M256F pointBNdcNegativeW = _mm256_xor_ps(_mm256_set1_ps(-0.0f), clipspaceVertexW[1]);
	const culling::M256F pointCNdcW = clipspaceVertexW[2];
	const culling::M256F pointCNdcNegativeW = _mm256_xor_ps(_mm256_set1_ps(-0.0f), clipspaceVertexW[2]);

	const culling::M256F pointANdcX = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexX[0], pointANdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexX[0], pointANdcW, _CMP_LE_OQ)); // make positive values ( https://stackoverflow.com/questions/23847377/how-does-this-function-compute-the-absolute-value-of-a-float-through-a-not-and-a )
	const culling::M256F pointBNdcX = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexX[1], pointBNdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexX[1], pointBNdcW, _CMP_LE_OQ));
	const culling::M256F pointCNdcX = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexX[2], pointCNdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexX[2], pointCNdcW, _CMP_LE_OQ));
	const culling::M256F pointANdcY = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexY[0], pointANdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexY[0], pointANdcW, _CMP_LE_OQ));
	const culling::M256F pointBNdcY = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexY[1], pointBNdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexY[1], pointBNdcW, _CMP_LE_OQ));
	const culling::M256F pointCNdcY = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexY[2], pointCNdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexY[2], pointCNdcW, _CMP_LE_OQ));
	const culling::M256F pointANdcZ = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexZ[0], pointANdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexZ[0], pointANdcW, _CMP_LE_OQ));
	const culling::M256F pointBNdcZ = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexZ[1], pointBNdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexZ[1], pointBNdcW, _CMP_LE_OQ));
	const culling::M256F pointCNdcZ = _mm256_and_ps(_mm256_cmp_ps(clipspaceVertexZ[2], pointCNdcNegativeW, _CMP_GE_OQ), _mm256_cmp_ps(clipspaceVertexZ[2], pointCNdcW, _CMP_LE_OQ));
	
	culling::M256I pointAInFrustum = _mm256_and_si256(_mm256_and_si256(*reinterpret_cast<const culling::M256I*>(&pointANdcX), *reinterpret_cast<const culling::M256I*>(&pointANdcY)), *reinterpret_cast<const culling::M256I*>(&pointANdcZ));
	culling::M256I pointBInFrustum = _mm256_and_si256(_mm256_and_si256(*reinterpret_cast<const culling::M256I*>(&pointBNdcX), *reinterpret_cast<const culling::M256I*>(&pointBNdcY)), *reinterpret_cast<const culling::M256I*>(&pointBNdcZ));
	culling::M256I pointCInFrustum = _mm256_and_si256(_mm256_and_si256(*reinterpret_cast<const culling::M256I*>(&pointCNdcX), *reinterpret_cast<const culling::M256I*>(&pointCNdcY)), *reinterpret_cast<const culling::M256I*>(&pointCNdcZ));

	// TODO : implemnet Clipping
	// Currently when w value is negative, it produce weird screen space value. https://stackoverflow.com/a/20180585
	// So i exclude triangle that any vertex is out of bound ( -w ~ w )
	const culling::M256I verticesInFrustum = _mm256_and_si256(_mm256_and_si256(*reinterpret_cast<const culling::M256I*>(&pointAInFrustum), *reinterpret_cast<const culling::M256I*>(&pointBInFrustum)), *reinterpret_cast<const culling::M256I*>(&pointCInFrustum));

	triangleCullMask &= _mm256_movemask_ps(*reinterpret_cast<const culling::M256F*>(&verticesInFrustum));
}



FORCE_INLINE void culling::BinTrianglesStage::BackfaceCulling
(
	culling::M256F* const screenPixelX,
	culling::M256F* const screenPixelY,
	std::uint32_t& triangleCullMask
)
{
	culling::M256F triArea1 = _mm256_mul_ps(_mm256_sub_ps(screenPixelX[1], screenPixelX[0]), _mm256_sub_ps(screenPixelY[2], screenPixelY[0]));
	culling::M256F triArea2 = _mm256_mul_ps(_mm256_sub_ps(screenPixelX[0], screenPixelX[2]), _mm256_sub_ps(screenPixelY[0], screenPixelY[1]));
	culling::M256F triArea = _mm256_sub_ps(triArea1, triArea2);
	culling::M256F ccwMask = _mm256_cmp_ps(triArea, _mm256_setzero_ps(), _CMP_GT_OQ);
	
	// Return a lane mask with all front faces set
	triangleCullMask &= _mm256_movemask_ps(ccwMask);
}



FORCE_INLINE void culling::BinTrianglesStage::PassTrianglesToTileBin
(
	const culling::M256F& pointAScreenPixelPosX,
	const culling::M256F& pointAScreenPixelPosY,
	const culling::M256F& pointANdcSpaceVertexZ,

	const culling::M256F& pointBScreenPixelPosX,
	const culling::M256F& pointBScreenPixelPosY,
	const culling::M256F& pointBNdcSpaceVertexZ,

	const culling::M256F& pointCScreenPixelPosX,
	const culling::M256F& pointCScreenPixelPosY,
	const culling::M256F& pointCNdcSpaceVertexZ,

	const std::uint32_t& triangleCullMask, 
	const size_t triangleCountPerLoop,
	const culling::M256I& outBinBoundingBoxMinX, 
	const culling::M256I& outBinBoundingBoxMinY,
	const culling::M256I& outBinBoundingBoxMaxX,
	const culling::M256I& outBinBoundingBoxMaxY
)
{
	for (size_t triangleIndex = 0; triangleIndex < triangleCountPerLoop; triangleIndex++)
	{
		if ((triangleCullMask & (1 << triangleIndex)) != 0x00000000)
		{
			const int intersectingMinBoxX = (reinterpret_cast<const int*>(&outBinBoundingBoxMinX))[triangleIndex]; // this is screen space coordinate
			const int intersectingMinBoxY = (reinterpret_cast<const int*>(&outBinBoundingBoxMinY))[triangleIndex];
			const int intersectingMaxBoxX = (reinterpret_cast<const int*>(&outBinBoundingBoxMaxX))[triangleIndex];
			const int intersectingMaxBoxY = (reinterpret_cast<const int*>(&outBinBoundingBoxMaxY))[triangleIndex];

			assert(intersectingMinBoxX <= intersectingMaxBoxX);
			assert(intersectingMinBoxY <= intersectingMaxBoxY);

			const int startBoxIndexX = MIN((int)(mMaskedOcclusionCulling->mDepthBuffer.mResolution.mColumnTileCount - 1), intersectingMinBoxX / TILE_WIDTH);
			const int startBoxIndexY = MIN((int)(mMaskedOcclusionCulling->mDepthBuffer.mResolution.mRowTileCount - 1), intersectingMinBoxY / TILE_HEIGHT);
			const int endBoxIndexX = MIN((int)(mMaskedOcclusionCulling->mDepthBuffer.mResolution.mColumnTileCount - 1), intersectingMaxBoxX / TILE_WIDTH);
			const int endBoxIndexY = MIN((int)(mMaskedOcclusionCulling->mDepthBuffer.mResolution.mRowTileCount - 1), intersectingMaxBoxY / TILE_HEIGHT);

			assert(startBoxIndexX >= 0 && startBoxIndexX < (int)(mMaskedOcclusionCulling->mDepthBuffer.mResolution.mColumnTileCount));
			assert(startBoxIndexY >= 0 && startBoxIndexY < (int)(mMaskedOcclusionCulling->mDepthBuffer.mResolution.mRowTileCount));
			
			assert(endBoxIndexX >= 0 && endBoxIndexX <= (int)(mMaskedOcclusionCulling->mDepthBuffer.mResolution.mColumnTileCount));
			assert(endBoxIndexY >= 0 && endBoxIndexY <= (int)(mMaskedOcclusionCulling->mDepthBuffer.mResolution.mRowTileCount));

			for (size_t y = startBoxIndexY; y <= endBoxIndexY; y++)
			{
				for (size_t x = startBoxIndexX; x <= endBoxIndexX; x++)
				{
					Tile* const targetTile = mMaskedOcclusionCulling->mDepthBuffer.GetTile(y, x);

					const size_t triListIndex = targetTile->mBinnedTriangles.mCurrentTriangleCount++;

					//assert(triListIndex < BIN_TRIANGLE_CAPACITY_PER_TILE);
					if(triListIndex >= BIN_TRIANGLE_CAPACITY_PER_TILE)
					{
						return;
					}

					targetTile->mBinnedTriangles.VertexX[0][triListIndex] = (reinterpret_cast<const float*>(&pointAScreenPixelPosX))[triangleIndex];
					targetTile->mBinnedTriangles.VertexY[0][triListIndex] = (reinterpret_cast<const float*>(&pointAScreenPixelPosY))[triangleIndex];
					targetTile->mBinnedTriangles.VertexZ[0][triListIndex] = (reinterpret_cast<const float*>(&pointANdcSpaceVertexZ))[triangleIndex];

					targetTile->mBinnedTriangles.VertexX[1][triListIndex] = (reinterpret_cast<const float*>(&pointBScreenPixelPosX))[triangleIndex];
					targetTile->mBinnedTriangles.VertexY[1][triListIndex] = (reinterpret_cast<const float*>(&pointBScreenPixelPosY))[triangleIndex];
					targetTile->mBinnedTriangles.VertexZ[1][triListIndex] = (reinterpret_cast<const float*>(&pointBNdcSpaceVertexZ))[triangleIndex];

					targetTile->mBinnedTriangles.VertexX[2][triListIndex] = (reinterpret_cast<const float*>(&pointCScreenPixelPosX))[triangleIndex];
					targetTile->mBinnedTriangles.VertexY[2][triListIndex] = (reinterpret_cast<const float*>(&pointCScreenPixelPosY))[triangleIndex];
					targetTile->mBinnedTriangles.VertexZ[2][triListIndex] = (reinterpret_cast<const float*>(&pointCNdcSpaceVertexZ))[triangleIndex];
					
				}
			}
		}
	}
}




FORCE_INLINE void culling::BinTrianglesStage::GatherVertices
(
	const float* const vertices,
	const size_t verticeCount,
	const std::uint32_t* const vertexIndices, 
	const size_t indiceCount, 
	const size_t currentIndiceIndex, 
	const size_t vertexStrideByte, 
	const size_t fetchTriangleCount,
	culling::M256F* outVerticesX, 
	culling::M256F* outVerticesY, 
	culling::M256F* outVerticesZ
)
{
	assert(indiceCount % 3 == 0);
	assert(currentIndiceIndex % 3 == 0);
	assert(indiceCount != 0); // TODO : implement gatherVertices when there is no indiceCount
	
	//Gather Indices
	const std::uint32_t* currentVertexIndices = vertexIndices + currentIndiceIndex;
	const culling::M256I indiceIndexs = _mm256_set_epi32(21, 18, 15, 12, 9, 6, 3, 0);
	static const culling::M256I SIMD_LANE_MASK[9] = {
		_mm256_setr_epi32(0,  0,  0,  0,  0,  0,  0,  0),
		_mm256_setr_epi32(~0,  0,  0,  0,  0,  0,  0,  0),
		_mm256_setr_epi32(~0, ~0,  0,  0,  0,  0,  0,  0),
		_mm256_setr_epi32(~0, ~0, ~0,  0,  0,  0,  0,  0),
		_mm256_setr_epi32(~0, ~0, ~0, ~0,  0,  0,  0,  0),
		_mm256_setr_epi32(~0, ~0, ~0, ~0, ~0,  0,  0,  0),
		_mm256_setr_epi32(~0, ~0, ~0, ~0, ~0, ~0,  0,  0),
		_mm256_setr_epi32(~0, ~0, ~0, ~0, ~0, ~0, ~0,  0),
		_mm256_setr_epi32(~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0)
	};

	// Compute per-lane index list offset that guards against out of bounds memory accesses
	const culling::M256I safeIndiceIndexs = _mm256_and_si256(indiceIndexs, SIMD_LANE_MASK[fetchTriangleCount]);

	culling::M256I m256i_indices[3];
	//If stride is 7
	//Current Value 
	//m256i_indices[0] : 0 ( first vertex index ), 3, 6,  9, 12, 15, 18, 21
	//m256i_indices[1] : 1 ( second vertex index ), 4, 7, 10, 13, 16, 19, 22
	//m256i_indices[2] : 2, 5, 8, 11, 14, 17, 20, 23
	//Point1 indices of Triangles
	m256i_indices[0] = _mm256_i32gather_epi32(reinterpret_cast<const INT32*>(currentVertexIndices + 0), safeIndiceIndexs, 4); // why 4? -> vertexIndices is std::uint32_t( 4byte )
	//Point2 indices of Triangles
	m256i_indices[1] = _mm256_i32gather_epi32(reinterpret_cast<const INT32*>(currentVertexIndices + 1), safeIndiceIndexs, 4);
	//Point3 indices of Triangles
	m256i_indices[2] = _mm256_i32gather_epi32(reinterpret_cast<const INT32*>(currentVertexIndices + 2), safeIndiceIndexs, 4);

	if(vertexStrideByte > 0)
	{
		//Consider Stride
		//If StrideByte is 28
		//m256i_indices[0] : 0 * 28, 3 * 28, 6 * 28,  9 * 28, 12 * 28, 15 * 28, 18 * 28, 21 * 28
		//m256i_indices[1] : 1 * 28, 4 * 28, 7 * 28, 10 * 28, 13 * 28, 16 * 28, 19 * 28, 22 * 28
		//m256i_indices[2] : 2 * 28, 5 * 28, 8 * 28, 11 * 28, 14 * 28, 17 * 28, 20 * 28, 23 * 28
		const culling::M256I m256i_stride = _mm256_set1_epi32(static_cast<INT32>(vertexStrideByte));
		m256i_indices[0] = _mm256_mullo_epi32(m256i_indices[0], m256i_stride);
		m256i_indices[1] = _mm256_mullo_epi32(m256i_indices[1], m256i_stride);
		m256i_indices[2] = _mm256_mullo_epi32(m256i_indices[2], m256i_stride);
	}
	

	//Gather vertexs
	//Should consider vertexStride(vertices isn't stored contiguously because generally vertex datas is stored with uv, normal datas...) 
	//And Should consider z value
	for (size_t i = 0; i < 3; i++)
	{
		outVerticesX[i] = _mm256_i32gather_ps((float*)vertices, m256i_indices[i], 1);
		outVerticesY[i] = _mm256_i32gather_ps((float*)vertices + 1, m256i_indices[i], 1);
		outVerticesZ[i] = _mm256_i32gather_ps((float*)vertices + 2, m256i_indices[i], 1);
	}
}

void culling::BinTrianglesStage::ConvertToPlatformDepth(culling::M256F* const depth)
{

#if (NDC_RANGE == MINUS_ONE_TO_POSITIVE_ONE)
	for (size_t i = 0; i < 3; i++)
	{
		//depth[i]
	}
#endif
	
}

void culling::BinTrianglesStage::BinTriangleThreadJob(const size_t cameraIndex)
{
	while (true)
	{
		culling::EntityBlock* const nextEntityBlock = GetNextEntityBlock(cameraIndex, false);

		if (nextEntityBlock != nullptr)
		{
			for (size_t entityIndex = 0; entityIndex < nextEntityBlock->mCurrentEntityCount; entityIndex++)
			{
				if
					(
						(nextEntityBlock->GetIsCulled(entityIndex, cameraIndex) == false) &&
						(nextEntityBlock->GetIsOccluder(entityIndex, cameraIndex) == true)
						)
				{
					const culling::Mat4x4 modelToClipSpaceMatrix = mCullingSystem->GetCameraViewProjectionMatrix(cameraIndex) * (*reinterpret_cast<const culling::Mat4x4*>(nextEntityBlock->GetModelMatrix(entityIndex)));
					const VertexData& vertexData = nextEntityBlock->mVertexDatas[entityIndex];

					BinTriangles
					(
						reinterpret_cast<const float*>(vertexData.mVertices),
						vertexData.mVerticeCount,
						vertexData.mIndices,
						vertexData.mIndiceCount,
						vertexData.mVertexStride,
						modelToClipSpaceMatrix.data()
					);
				}
			}

		}
		else
		{
			break;
		}
	}
}

void culling::BinTrianglesStage::BinTriangleThreadJobByObjectOrder(const size_t cameraIndex)
{
	for (UINT32 layerIndex = 0; layerIndex < MAX_LAYER_COUNT; layerIndex++)
	{
		const std::vector<dooms::Renderer*>& renderersInLayer = dooms::RendererComponentStaticIterator::GetSingleton()->GetWorkingRendererInLayer(cameraIndex, layerIndex);

		for(INT64 rendererIndex = 0 ; rendererIndex < renderersInLayer.size() ; rendererIndex++)
		{
			dooms::Renderer* const renderer = renderersInLayer[rendererIndex];
			if
			(
				dooms::IsValid(renderer) == true &&
				renderer->mCullingEntityBlockViewer.GetIsCulled(cameraIndex) == false &&
				renderer->mCullingEntityBlockViewer.GetIsOccluder(cameraIndex) == true
			)
			{
				
				const culling::Mat4x4 modelToClipSpaceMatrix = mCullingSystem->GetCameraViewProjectionMatrix(cameraIndex) * (*reinterpret_cast<const culling::Mat4x4*>(renderer->mCullingEntityBlockViewer.GetModelMatrix()));
				const VertexData& vertexData = renderer->mCullingEntityBlockViewer.GetVertexData();

				BinTriangles
				(
					reinterpret_cast<const float*>(vertexData.mVertices),
					vertexData.mVerticeCount,
					vertexData.mIndices,
					vertexData.mIndiceCount,
					vertexData.mVertexStride,
					modelToClipSpaceMatrix.data()
				);
			}
		}
	}
}

culling::BinTrianglesStage::BinTrianglesStage(MaskedSWOcclusionCulling* mMOcclusionCulling)
	: MaskedSWOcclusionCullingStage{ mMOcclusionCulling }
{
}

void culling::BinTrianglesStage::ResetCullingModule()
{
	MaskedSWOcclusionCullingStage::ResetCullingModule();

	IsWorkingByAThread.store(false, std::memory_order_seq_cst);
}

void culling::BinTrianglesStage::CullBlockEntityJob(const size_t cameraIndex)
{
	const bool canWorkable = !(IsWorkingByAThread.exchange(true, std::memory_order_seq_cst));

	// Only one thread can work on this stage
	if(canWorkable == true)
	{
#ifdef FETCH_OBJECT_SORT_FROM_DOOMS_ENGINE_IN_BIN_TRIANGLE_STAGE
		BinTriangleThreadJobByObjectOrder(cameraIndex);
#else
		BinTriangleThreadJob(cameraIndex);
#endif
			
	}
}

FORCE_INLINE void culling::BinTrianglesStage::BinTriangles
(
	const float* const vertices, 
	const size_t verticeCount,
	const std::uint32_t* const vertexIndices, 
	const size_t indiceCount, 
	const size_t vertexStrideByte, 
	const float* const modelToClipspaceMatrix
)
{
	static constexpr std::int32_t triangleCountPerLoop = 8;

	std::int32_t currentIndiceIndex = -(triangleCountPerLoop * 3);

	assert(indiceCount != 0); // check GatherVertices function

	while (currentIndiceIndex < (std::int32_t)indiceCount)
	{
		currentIndiceIndex += (triangleCountPerLoop * 3);

		if(currentIndiceIndex >= (std::int32_t)indiceCount)
		{
			break;
		}
		const size_t fetchTriangleCount = MIN(8, (indiceCount - currentIndiceIndex) / 3);
		assert(fetchTriangleCount != 0);

		// First 4 bits show if traingle is valid
		// Current Value : 00000000 00000000 00000000 11111111
		std::uint32_t triangleCullMask = (1 << fetchTriangleCount) - 1;


		//Why Size of array is 3?
		//A culling::M256F can have 8 floating-point
		//A TwoDTriangle have 3 point
		//So If you have just one culling::M256F variable, a floating-point is unused.
		//Not to make unused space, 12 floating point is required per axis
		// culling::M256F * 3 -> 8 TwoDTriangle with no unused space

		//We don't need z value in Binning stage!!!
		// Triangle's First Vertex X is in ndcSpaceVertexX[0][0]
		// Triangle's Second Vertex X is in ndcSpaceVertexX[0][1]
		// Triangle's Third Vertex X is in ndcSpaceVertexX[0][2]
		culling::M256F ndcSpaceVertexX[3], ndcSpaceVertexY[3], ndcSpaceVertexZ[3], oneDividedByW[3];
		
		//Gather Vertex with indice
		//WE ARRIVE AT MODEL SPACE COORDINATE!
		GatherVertices(vertices, verticeCount, vertexIndices, indiceCount, currentIndiceIndex, vertexStrideByte, fetchTriangleCount, ndcSpaceVertexX, ndcSpaceVertexY, ndcSpaceVertexZ);
		
		//Convert Model space Vertex To Clip space Vertex
		//WE ARRIVE AT CLIP SPACE COORDINATE. W IS NOT 1
		culling::vertexTransformationHelper::TransformThreeVerticesToClipSpace(ndcSpaceVertexX, ndcSpaceVertexY, ndcSpaceVertexZ, oneDividedByW, modelToClipspaceMatrix);
		
		// Clipping befor ndc https://stackoverflow.com/questions/41085117/why-does-gl-divide-gl-position-by-w-for-you-rather-than-letting-you-do-it-your
		Clipping(ndcSpaceVertexX, ndcSpaceVertexY, ndcSpaceVertexZ, oneDividedByW, triangleCullMask);

		if (triangleCullMask == 0x00000000)
		{
			continue;
		}

		oneDividedByW[0] = culling::M256F_DIV(_mm256_set1_ps(1.0f), oneDividedByW[0]);
		oneDividedByW[1] = culling::M256F_DIV(_mm256_set1_ps(1.0f), oneDividedByW[1]);
		oneDividedByW[2] = culling::M256F_DIV(_mm256_set1_ps(1.0f), oneDividedByW[2]);
		
		culling::M256F screenPixelPosX[3], screenPixelPosY[3];
		culling::vertexTransformationHelper::ConvertClipSpaceThreeVerticesToScreenPixelSpace(ndcSpaceVertexX, ndcSpaceVertexY, oneDividedByW, screenPixelPosX, screenPixelPosY, mMaskedOcclusionCulling->mDepthBuffer);

		BackfaceCulling(screenPixelPosX, screenPixelPosY, triangleCullMask);

		if (triangleCullMask == 0x00000000)
		{
			continue;
		}
		

		Sort_8_3DTriangles
		(
			screenPixelPosX[0], 
			screenPixelPosY[0], 
			ndcSpaceVertexZ[0],

			screenPixelPosX[1],
			screenPixelPosY[1],
			ndcSpaceVertexZ[1],

			screenPixelPosX[2],
			screenPixelPosY[2],
			ndcSpaceVertexZ[2]
		);

		
		culling::M256F LEFT_MIDDLE_POINT_X;
		culling::M256F LEFT_MIDDLE_POINT_Y;
		culling::M256F LEFT_MIDDLE_POINT_Z;

		culling::M256F RIGHT_MIDDLE_POINT_X;
		culling::M256F RIGHT_MIDDLE_POINT_Y;
		culling::M256F RIGHT_MIDDLE_POINT_Z;

		
		// split triangle
		culling::rasterizerHelper::GetMiddlePointOfTriangle
		(
			screenPixelPosX[0],
			screenPixelPosY[0],
			ndcSpaceVertexZ[0],

			screenPixelPosX[1],
			screenPixelPosY[1],
			ndcSpaceVertexZ[1],

			screenPixelPosX[2],
			screenPixelPosY[2],
			ndcSpaceVertexZ[2],

			LEFT_MIDDLE_POINT_X,
			LEFT_MIDDLE_POINT_Y,
			LEFT_MIDDLE_POINT_Z,

			RIGHT_MIDDLE_POINT_X,
			RIGHT_MIDDLE_POINT_Y,
			RIGHT_MIDDLE_POINT_Z
		);



		{
			//Bin Bottom Flat Triangle


			culling::M256I outBinBoundingBoxMinX, outBinBoundingBoxMinY, outBinBoundingBoxMaxX, outBinBoundingBoxMaxY;
			//Bin Triangles to tiles

			//Compute Bin Bounding Box
			//Get Intersecting Bin List
			culling::depthBufferTileHelper::ComputeBinBoundingBoxFromThreeVertices
			(
				screenPixelPosX[0],
				screenPixelPosY[0],

				LEFT_MIDDLE_POINT_X,
				LEFT_MIDDLE_POINT_Y,

				RIGHT_MIDDLE_POINT_X,
				RIGHT_MIDDLE_POINT_Y,

				outBinBoundingBoxMinX,
				outBinBoundingBoxMinY,
				outBinBoundingBoxMaxX,
				outBinBoundingBoxMaxY,
				mMaskedOcclusionCulling->mDepthBuffer
			);

			// Pass triangle in counter clock wise
			PassTrianglesToTileBin
			(
				screenPixelPosX[0],
				screenPixelPosY[0],
				ndcSpaceVertexZ[0],

				LEFT_MIDDLE_POINT_X,
				LEFT_MIDDLE_POINT_Y,
				LEFT_MIDDLE_POINT_Z,

				RIGHT_MIDDLE_POINT_X,
				RIGHT_MIDDLE_POINT_Y,
				RIGHT_MIDDLE_POINT_Z,

				triangleCullMask,
				triangleCountPerLoop,
				outBinBoundingBoxMinX,
				outBinBoundingBoxMinY,
				outBinBoundingBoxMaxX,
				outBinBoundingBoxMaxY
			);
		}


		{
			//Bin Top Flat Triangle

			culling::M256I outBinBoundingBoxMinX, outBinBoundingBoxMinY, outBinBoundingBoxMaxX, outBinBoundingBoxMaxY;
		
			culling::depthBufferTileHelper::ComputeBinBoundingBoxFromThreeVertices
			(
				screenPixelPosX[2],
				screenPixelPosY[2],

				RIGHT_MIDDLE_POINT_X,
				RIGHT_MIDDLE_POINT_Y,

				LEFT_MIDDLE_POINT_X,
				LEFT_MIDDLE_POINT_Y,
				
				outBinBoundingBoxMinX,
				outBinBoundingBoxMinY,
				outBinBoundingBoxMaxX,
				outBinBoundingBoxMaxY,
				mMaskedOcclusionCulling->mDepthBuffer
			);

			// Pass triangle in counter clock wise
			PassTrianglesToTileBin
			(
				screenPixelPosX[2],
				screenPixelPosY[2],
				ndcSpaceVertexZ[2],

				RIGHT_MIDDLE_POINT_X,
				RIGHT_MIDDLE_POINT_Y,
				RIGHT_MIDDLE_POINT_Z,

				LEFT_MIDDLE_POINT_X,
				LEFT_MIDDLE_POINT_Y,
				LEFT_MIDDLE_POINT_Z,
				
				triangleCullMask,
				triangleCountPerLoop,
				outBinBoundingBoxMinX,
				outBinBoundingBoxMinY,
				outBinBoundingBoxMaxX,
				outBinBoundingBoxMaxY
			);
		}
		
		
	}
}

