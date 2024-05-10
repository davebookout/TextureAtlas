#include "Allocator2D.h"
#include <vector>
// tree of 2^n elements has n levels
// 4k texture is 2^12 -- 4 bits to store 12 levels
// 
// PPPP LLLL XXXX XXXX XXXX YYYY YYYY YYYY
// page, level, x offset, y offset.


struct Allocator2DImpl {
	std::vector<Allocation2D> free;
	std::vector<Allocation2D> used;
};

Allocator2D::Allocator2D() {

}

// Creates a new allocator dimension x dimension
// and adds a single dimension x dimension storage block.

Allocator2D::Allocator2D(unsigned int dimension) : mDimension(dimension), mAllocatedPages(0), mFree(1+ceil(log2(dimension))) {
	mFree[0].insert({ 0, 0, 0, 0 });
	mAllocatedPages++;
}

// Allocate returns an AllocationID for an allocation satisfying blocksX x blocksY
AllocationID Allocator2D::Allocate(int blocksX, int blocksY) {//, bool alignToSize) {
	if (blocksX > mDimension || blocksY > mDimension) {
		return AllocationID();
	}
	int levelX = AllocationLevel(blocksX);
	int levelY = AllocationLevel(blocksY);
	if (levelX == levelY) {
		int allocLevel = levelX;
		int smallestAvailable = allocLevel;
		while (mFree[smallestAvailable].empty() && smallestAvailable > 0) {
			smallestAvailable--;
		}
		if (smallestAvailable == 0 && mFree[smallestAvailable].empty()) {
			mFree[smallestAvailable].insert({ mAllocatedPages, 0, 0, 0 });
			mAllocatedPages++;
		}
		while (smallestAvailable < allocLevel) {
			SplitAllocation(smallestAvailable);
			smallestAvailable++;
		}
		auto iter = mFree[smallestAvailable].begin();
		AllocationID newId = AllocationID(*iter);
		mFree[smallestAvailable].erase(iter);
		return newId;
	}
	else if (levelX > levelY) {
		//vertical rectangle
		unsigned int levelIncrX = LevelOffsetIncr(levelY + 1);
		unsigned int levelIncrY = LevelOffsetIncr(levelY);
		AllocationID testId = AllocationID(0, levelY + 1, 0, 0);
		for (auto test : mFree[levelY + 1]) {
			AllocationID testID = AllocationID(test);
			if (testID.offsetY & ~levelIncrY) {
				AllocationID nextID = AllocationID(testID);
				nextID.offsetY += levelIncrX;
				auto nextItr = mFree[levelY + 1].find(nextID.AsUint());
				if (nextItr != mFree[levelY + 1].end()) {
					mFree[levelY + 1].erase(test);
					mFree[levelY + 1].erase(nextItr);
					return testID;
				}
			}
		}
		AllocationID newId = Allocate(blocksY, blocksY);
		if (!newId.Valid())
			return AllocationID();
		
		std::vector<AllocationID> subAllocations;
		subAllocations.push_back(newId);

		for (int i = 0; i < levelX - levelY; i++) {
			for (AllocationID id : subAllocations) {
				std::vector<AllocationID> splits = SplitAllocation(newId);
				for (AllocationID id : splits) {
					if (id.offsetX != newId.offsetX) {
						AddAllocation(id);
					}
					else {
						subAllocations.push_back(id);
					}
				}
			}
			std::vector<AllocationID> subAllocations = SplitAllocation(newId);
		}
		return newId;
	} 
	else {
		// horizontal rectangle
		unsigned int levelIncrX = LevelOffsetIncr(levelX);
		unsigned int levelIncrY = LevelOffsetIncr(levelX+1);
		AllocationID testId = AllocationID(0, levelX + 1, 0, 0);
		for (auto test : mFree[levelX+1]) {
			AllocationID testID = AllocationID(test);
			if (testID.offsetX & ~levelIncrX) {
				AllocationID nextID = AllocationID(testID);
				nextID.offsetX += levelIncrY;
				auto nextItr = mFree[levelX + 1].find(nextID.AsUint());
				if (nextItr != mFree[levelX + 1].end()) {
					mFree[levelX + 1].erase(test);
					mFree[levelX + 1].erase(nextItr);
					return testID;
				}
			}
		}

		AllocationID newId = Allocate(blocksX, blocksX);
		if (!newId.Valid())
			return AllocationID();
		std::vector<AllocationID> subAllocations;
		subAllocations.push_back(newId);

		for (int i = 0; i < levelY - levelX; i++) {
			for (AllocationID id : subAllocations) {
				std::vector<AllocationID> splits = SplitAllocation(newId);
				for (AllocationID id : splits) {
					if (id.offsetY != newId.offsetY) {
						AddAllocation(id);
					}
					else {
						subAllocations.push_back(id);
					}
				}
			}
		}
		return newId;
	}
}

//	SplitAllocation splits an allocation at level into
//		4 allocations that it adds to the next level
void Allocator2D::SplitAllocation(unsigned int level) {
	if (mFree[level].empty())
		return;
	auto idItr = mFree[level].begin();
	AllocationID id = *idItr;
	mFree[level].erase(idItr);
	AllocationID newId = AllocationID(id);
	unsigned int newLevel = id.level + 1;
	if (!(newLevel < mFree.size())) {
		return;
	}
	newId.level = newLevel;
	unsigned int offsetMod = LevelOffsetIncr(newLevel);
	AddAllocation(newId);
	newId.offsetY += offsetMod;
	AddAllocation(newId);
	newId.offsetX += offsetMod;
	AddAllocation(newId);
	newId.offsetY -= offsetMod;
	AddAllocation(newId);
}

std::vector<AllocationID> Allocator2D::SplitAllocation(AllocationID id) {
	AllocationID newId = AllocationID(id);
	unsigned int newLevel = id.level + 1;
	if (!(newLevel < mFree.size())) {
		return {};
	}
	std::vector<AllocationID> result;
	newId.level = newLevel;
	int offsetMod = 1 << (mFree.size() - 1 - (newLevel));
	result.push_back(newId);
	newId.offsetY += offsetMod;
	result.push_back(newId);
	newId.offsetX += offsetMod;
	result.push_back(newId);
	newId.offsetY -= offsetMod;
	result.push_back(newId);
	RemoveAllocation(id);
	return result;
}

void Allocator2D::AddAllocation(AllocationID id) {
	mFree[id.level].insert(id.AsUint());
}

void Allocator2D::RemoveAllocation(AllocationID id) {
	mFree[id.level].erase(id.AsUint());
}

AllocationID::AllocationID() : page(-1), level(-1), offsetX(-1), offsetY(-1) {};
AllocationID::AllocationID(unsigned int id) {
	page = id >> 28;
	level = (id >> 24) & 0xF;
	offsetX = (id >> 12) & 0xFFF;
	offsetY = id & 0xFFF;
}

AllocationID::AllocationID(unsigned int page, unsigned int level, unsigned int offsetX, unsigned int offsetY)
	: page(page), level(level), offsetX(offsetX), offsetY(offsetY) {};
const bool AllocationID::Valid() {
	return page != -1 || level != -1 || offsetX != -1 || offsetY != -1;
}
const bool AllocationID::Equal(const AllocationID& cmp) {
	return page == cmp.page && level == cmp.level && offsetX == cmp.offsetX && offsetY == cmp.offsetY;
}
unsigned int AllocationID::AsUint() { return  (page << 28) | (level << 24) | (offsetX << 12) | offsetY; };



class Allocator2DSimple {
	int mBlockSize;
	std::array<unsigned int, 32> mMemory;
	//keep it simple with 32x32 block allocation	
public:
	Allocator2DSimple() : mBlockSize(32), mMemory(array<unsigned int, 32>()) {
		for (unsigned int& m : mMemory) {
			m = 0xFFFFFFFF;
		}
	};
	bool Available(int x, int y, int offset[2]) {
		if (x + offset[0] >= mBlockSize || y + offset[1] >= mBlockSize)
			return false;
		unsigned int mask = (1 << x) - 1;
		mask = mask << offset[0];
		for (int j = offset[1]; j < offset[1] + y; j++) {
			if ((mMemory[j] & mask) != mask) {
				return false;
			}
		}
		return true;
	}

	void MarkUsed(int x, int y, int offset[2]) {
		unsigned int m = ~(((1 << x) - 1) << offset[0]);
		for (int j = offset[1]; j < offset[1] + y; j++) {
			mMemory[j] = (mMemory[j] & m);
		}
	}

	bool Allocate(int blocksX, int blocksY, int outOffset[2]) {
		//just grab first top left available...
		unsigned int allocate = (1 << blocksX) - 1;
		outOffset[0] = 0; outOffset[1] = 0;
		while (outOffset[1] < mBlockSize) {
			if (Available(blocksX, blocksY, outOffset))
			{
				MarkUsed(blocksX, blocksY, outOffset);
				return true;
			}
			outOffset[1]++;
			if (outOffset[1] == mBlockSize) {
				if (outOffset[0] < mBlockSize - 1) {
					outOffset[0]++;
					outOffset[1] = 0;
				}
			}
		}
		return false;
	}
};
