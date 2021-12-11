#include "MaskedSWOcclusionCulling.h"

#include "Stage/BinTrianglesStage.h"
#include "Stage/RasterizeTrianglesStage.h"

void culling::MaskedSWOcclusionCulling::ResetDepthBuffer()
{
	const size_t tileCount = static_cast<size_t>(mDepthBuffer.mResolution.mTileCountInARow) * static_cast<size_t>(mDepthBuffer.mResolution.mTileCountInAColumn);
	std::shared_ptr<Tile[]> tiles = mDepthBuffer.mTiles;
	
	for (size_t i = 0; i < tileCount; i++)
	{
		tiles[i].mBinnedTriangles.mCurrentTriangleCount = 0;
		tiles[i].mHizDatas.depthPosition = _mm256_set1_epi32(0);
		tiles[i].mHizDatas.z0Max = _mm256_set1_ps(1.0f);
		tiles[i].mHizDatas.z1Max = _mm256_set1_ps(1.0f);
	}
	
}

void culling::MaskedSWOcclusionCulling::DrawOccluderTriangles
(
	const float* vertices, const std::uint32_t* vertexIndices,
	size_t indiceCount, bool vertexStrideByte, float* modelToClipspaceMatrix
)
{
	mBinTrianglesStage.BinTriangles(vertices, vertexIndices, indiceCount, vertexStrideByte, modelToClipspaceMatrix);

	//mRasterizeTrianglesStage.RasterizeBinnedTriangles();
}

void culling::MaskedSWOcclusionCulling::DepthTestOccludee
(
	const AABB* worldAABBs, 
	size_t aabbCount,
	char* visibleBitFlags
)
{
	//Check Area of TwoDTriangle
	//if Area is smaller than setting, Dont draw that occluder
	//Occluder should be huge to occlude occludee

	//Compute BoundingBoxs(Sub Tile)
	//

	//!!!!!!!
	//We Just need Minimum Depth Of AABB ( regardless with Tile )
	//And Just Compare this minimum depth with Max depth of Tiles
	//Dont Try drawing triangles of AABB, Just Compute MinDepth of AABB

	culling::M256F aabbVertexX[3];
	culling::M256F aabbVertexY[3];
	culling::M256F aabbVertexZ[3];

	///Temporarily remove unused varaible warning
	(void)aabbVertexX, (void)aabbVertexY, (void)aabbVertexZ;

	// 3 AABB is checked per loop
	for (size_t i = 0; i < aabbCount; i += 3)
	{
		// ComputeMinimumDepths
	}



}

culling::MaskedSWOcclusionCulling::MaskedSWOcclusionCulling(EveryCulling* frotbiteCullingSystem, std::uint32_t depthBufferWidth, std::uint32_t depthBufferheight)
	: culling::CullingModule{frotbiteCullingSystem}, 
	mDepthBuffer {
	depthBufferWidth, depthBufferheight
}, binCountInRow{ depthBufferWidth / SUB_TILE_WIDTH }, binCountInColumn{ depthBufferheight / SUB_TILE_HEIGHT },
	mBinTrianglesStage{*this}, mRasterizeTrianglesStage{*this}
{
	assert(depthBufferWidth% TILE_WIDTH == 0);
	assert(depthBufferheight% TILE_HEIGHT == 0);
}

void culling::MaskedSWOcclusionCulling::SetNearFarClipPlaneDistance(float nearClipPlaneDis, float farClipPlaneDis)
{
	mNearClipPlaneDis = nearClipPlaneDis;
	mFarClipPlaneDis = farClipPlaneDis;
}

void culling::MaskedSWOcclusionCulling::SetViewProjectionMatrix(float* viewProjectionMatrix)
{
	mViewProjectionMatrix = viewProjectionMatrix;
}

void culling::MaskedSWOcclusionCulling::ResetState()
{
	ResetDepthBuffer();

}

void culling::MaskedSWOcclusionCulling::CullBlockEntityJob(EntityBlock* currentEntityBlock, size_t entityCountInBlock, size_t cameraIndex)
{
}




	


