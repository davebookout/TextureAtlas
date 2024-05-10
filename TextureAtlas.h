#pragma once
struct TextureData {
	int size[2];
	int rowPitch;
	BYTE* pData;
};
struct Texture {
	int size[2];
	int channels;
	int format; // assuming 4 channel uncompressed
	int mips;
	unsigned int id;
	char fileName[256];
	std::vector<TextureData> mipData;
};

struct SampleParameters {
	float start[2] = { 1.0, 0.0 };
	float end[2] = { 0.0, 1.0 };
	int index = 0;
	int padding[3];
};

struct TextureAtlas {
	int width = 0;
	int height = 0;
	int slices = 0;
	std::vector<Texture> textures;
	std::vector<SampleParameters> sampleParameters;
};

TextureAtlas CreateTextureAtlas(std::vector<Texture>& textures, int baseTextureSize);
