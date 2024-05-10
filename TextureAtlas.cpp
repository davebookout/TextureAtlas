#include "stdafx.h"
#include "TextureAtlas.h"
#include "Allocator2D.h"

void CopyIntoImage(BYTE* dst, int dstPitch, int dstOffsetX, int dstOffsetY, BYTE* src, int srcPitch, int rows);
void DownScale(TextureData& src, TextureData& dst, bool zeroOneAlpha);

TextureAtlas CreateTextureAtlas(std::vector<Texture>& textures, int baseTextureSize) {

	Allocator2D allocator(baseTextureSize / 32);
	TextureAtlas result;
	result.height = baseTextureSize;
	result.width = baseTextureSize;
	result.slices = 1;
	int blockSizeTexels = baseTextureSize / 32;
	std::vector<SampleParameters> sampleParameters;

	for (Texture& t : textures) {
		int nextMipWidth = t.size[0] >> 1;
		int nextMipHeight = t.size[1] >> 1;
		int nextMip = 1;
		while (nextMipWidth > 1 && nextMipHeight > 1) {
			TextureData data;
			data.rowPitch = nextMipWidth * 4;
			data.size[0] = nextMipWidth;
			data.size[1] = nextMipHeight;
			data.pData = (BYTE*)malloc(data.rowPitch * data.size[1]);
			t.mipData.push_back(std::move(data));
			DownScale(t.mipData[nextMip - 1], t.mipData[nextMip], true);
			nextMipWidth = nextMipWidth >> 1;
			nextMipHeight = nextMipHeight >> 1;
			nextMip++;
		}
		t.mips = t.mipData.size();
	}

	for (int i = 0; i < textures.size(); i++) {
		int width = textures[i].size[0];
		int height = textures[i].size[1];
		int blocksWidth = ceil(1.0 * width / blockSizeTexels);
		int blocksHeight = ceil(1.0 * height / blockSizeTexels);
		AllocationID allocId = allocator.Allocate(blocksWidth, blocksHeight);
		if (!allocId.Valid()) {
			result.sampleParameters.push_back({});
		}
		else {
			if (allocId.page >= result.textures.size()) {
				int prevSize = result.textures.size();
				result.textures.resize(allocId.page + 1);
				for (int newTexture = prevSize; newTexture <= allocId.page; newTexture++) {
					result.textures[newTexture].channels = 4;
					result.textures[newTexture].format = 0;
					result.textures[newTexture].id = newTexture;
					result.textures[newTexture].size[0] = baseTextureSize;
					result.textures[newTexture].size[1] = baseTextureSize;
					for (int mip = baseTextureSize; mip > 1; mip = mip / 2) {
						TextureData d;
						d.rowPitch = mip * result.textures[0].channels;
						d.size[0] = mip;
						d.size[1] = mip;
						d.pData = (BYTE*)malloc(d.rowPitch * d.size[1]);
						result.textures[newTexture].mipData.push_back(std::move(d));
					}
					result.textures[newTexture].mips = result.textures[newTexture].mipData.size();
				}
			}
			SampleParameters p;
			p.index = allocId.page;
			p.start[0] = (float)blockSizeTexels * allocId.offsetX / baseTextureSize;
			p.start[1] = (float)blockSizeTexels * allocId.offsetY / baseTextureSize;
			p.end[0] = (float)(blockSizeTexels * allocId.offsetX + width - 1) / baseTextureSize;
			p.end[1] = (float)(blockSizeTexels * allocId.offsetY + height - 1) / baseTextureSize;

			int dstPitch = 4 * baseTextureSize;
			sampleParameters.push_back(p);
			for (int mip = 0; mip < textures[i].mipData.size(); mip++) {
				int offsetX = (allocId.offsetX * blockSizeTexels * textures[0].channels) >> mip;
				int offsetY = (blockSizeTexels * allocId.offsetY) >> mip;
				CopyIntoImage(result.textures[p.index].mipData[mip].pData,
					result.textures[p.index].mipData[mip].rowPitch,
					offsetX, offsetY,
					textures[i].mipData[mip].pData,
					textures[i].mipData[mip].rowPitch,
					textures[i].mipData[mip].size[1]);

			}
		}
	}
}

void CopyIntoImage(BYTE* dst, int dstPitch, int dstOffsetX, int dstOffsetY, BYTE* src, int srcPitch, int rows) {
	int copySize = min(dstPitch - dstOffsetX, srcPitch);
	for (int copyRow = 0; copyRow < rows; copyRow++) {
		BYTE* copyDst = dst + dstPitch * (dstOffsetY + copyRow) + dstOffsetX;
		BYTE* copySrc = src + srcPitch * copyRow;
		memcpy(copyDst, copySrc, copySize);
	}
}


void DownScale(TextureData& src, TextureData& dst, bool zeroOneAlpha) {
	int blockSizeX = src.size[0] / dst.size[0];
	int blockSizeY = src.size[1] / dst.size[1];

	for (int bX = 0; bX < src.size[0] / blockSizeX; bX++) {
		for (int bY = 0; bY < src.size[1] / blockSizeY; bY++) {
			UINT block[] = { 0, 0, 0, 0 };
			int xOffset = bX * blockSizeX;
			int yOffset = bY * blockSizeY;
			for (int x = 0; x < blockSizeX; x++) {
				for (int y = 0; y < blockSizeY; y++) {
					int p = (x + xOffset) * 4 + (y + yOffset) * src.rowPitch;
					UINT alpha = (src.pData[p + 3]); ;
					block[0] += alpha * (src.pData[p]);
					block[1] += alpha * (src.pData[p + 1]);
					block[2] += alpha * (src.pData[p + 2]);
					block[3] += alpha;
				}
			}
			UINT alpha = block[3];
			if (alpha > 0) {
				block[0] /= alpha;
				block[1] /= alpha;
				block[2] /= alpha;
				block[3] /= (blockSizeX * blockSizeY);
			}
			int p = dst.rowPitch * bY + bX * 4;
			dst.pData[p] = block[0];
			dst.pData[p + 1] = block[1];
			dst.pData[p + 2] = block[2];
			dst.pData[p + 3] = block[3];
		}
	}
	// todo check edges
}
