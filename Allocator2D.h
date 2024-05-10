#pragma once
#include <unordered_set>
#include <vector>

// allocations are in "blocks"
// values are in "blocks" units
// interpreting block size is up to user of class
//  
struct Allocation2D {
	unsigned int offsetX;
	unsigned int offsetY;
	unsigned int width;
	unsigned int height;
	unsigned int sourceID;
	unsigned int ID;
	bool valid;
};

struct AllocationID {
	unsigned int page : 4;
	unsigned int level : 4;
	unsigned int offsetX : 12;
	unsigned int offsetY : 12;
	AllocationID();
	AllocationID(unsigned int id);
	AllocationID(unsigned int page, unsigned int level, unsigned int offsetX, unsigned int offsetY);
	const bool Valid();
	const bool Equal(const AllocationID& cmp);
	unsigned int AsUint();
};

//Allocates 2D blocks
 //constrained square and power of 2 for storage
 //and allocation
 //max size storage page size is 4096 "blocks"
class Allocator2D {
public:
	Allocator2D(unsigned int dimension);
	AllocationID Allocate(int blocksX, int blocksY);

private:
	int mDimension;
	unsigned int  mAllocatedPages;
	std::vector<std::unordered_set<unsigned int>> mFree;

	Allocator2D();
	void SplitAllocation(unsigned int);
	std::vector<AllocationID> SplitAllocation(AllocationID a);
	unsigned int AllocationLevel(unsigned int blocks) {
		return mFree.size() - 1 - static_cast<unsigned int>(ceil(log2(blocks)));
	}
	void AddAllocation(AllocationID id);
	void RemoveAllocation(AllocationID id);
	unsigned int LevelOffsetIncr(unsigned int level) {
		return 1 << (mFree.size() - 1 - level);
	}
};
