//
// Contains game-code-independent utilities and foundamental defines.
//

#ifndef BI_BASE_H
#define BI_BASE_H

//
// Types
//
#include <stdint.h>
typedef float     f32;
typedef double    f64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef u8        b8;
typedef u16       b16;
typedef u32       b32;
typedef u64       b64;
typedef intptr_t  smm; // Can represent both a memory position, and memory size. (32-bit: 4 bytes; 64-bit: 8 bytes)
typedef uintptr_t umm; // Can represent both a memory position, and memory size. (32-bit: 4 bytes; 64-bit: 8 bytes)


#include <string.h>

//
// Defines
//
#ifndef NO_ASSERTS
	#define Assert(x) { if (!(x)) { *(int *)0 = 0; } }
#else
	#define Assert(x)
#endif
#define AssertRange(x0, x1, x2) Assert(((x0) <= (x1)) && ((x1) <= (x2))) // Inclusive

#define OffsetOf(type, member) (umm)&(((type *)0)->member)
#define ArrayCount(arr) (sizeof(arr) / sizeof(arr[0]))

#define ArrayCopy(arrayFrom, arrayTo){ \
	for(int i = 0; i < ArrayCount(arrayFrom); i++){ \
		arrayTo[i] = arrayFrom[i]; \
	} \
}

// Uses the passed pointer's type to calculate the size.
#define ZeroStruct(pointer) ZeroSize((void *)(pointer), sizeof((pointer)[0]))

// Gets the type from the 'pointer' to calculate the size.
#define ZeroArrayPtr(pointer, count) ZeroSize((void *)(pointer), (count)*sizeof((pointer)[0]))

// 'actualArray' must be an ACTUAL ARRAY like int a[25], and it's type and count will be used to calculate the size.
#define ZeroArray(actualArray) ZeroSize((void *)(actualArray), sizeof(actualArray))
inline void ZeroSize(void *ptr, umm size){
	memset(ptr, 0, size);
}

#define SWAP(a, b) {auto a_ = a; a = b; b = a_;}

//
// Bit flags
//
inline b32 CheckFlagU8(u8 flags, s32 flagPos){
	b32 result = (flags & ((u8)1 << flagPos)) != 0;
	return result;
}
#define CheckFlag(flags, bitMask) ((flags) & (bitMask))
#define CheckFlagsOr(flags, bitMask) ((flags) & (bitMask))
#define CheckFlagsAnd(flags, bitMask) (((flags) & (bitMask)) == (bitMask))

// 'flags' is not a pointer.
#define SetFlag(flags, bitMask)	   {flags |=  bitMask;}
// 'flags' is not a pointer. Remember to separate the flags with '|'.
#define SetFlags SetFlag
// 'flags' is not a pointer.
#define UnsetFlag(flags, bitMask)   {flags &= ~(bitMask);}
// 'flags' is not a pointer. Remember to separate the flags with '|'.
#define UnsetFlags UnsetFlag
// 'flags' is not a pointer. Sets the flags if condition is true, unsets them otherwise.
#define SetOrUnsetFlag(flags, bitMask, condition) { flags = ((condition)? (flags)|(bitMask) : (flags)&~(bitMask)); }
// 'flags' is not a pointer. Remember to separate the flags with '|'.
#define SetOrUnsetFlags SetOrUnsetFlag

#include <stdio.h>



// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
struct pcg_random_state{
	u64 state;
	u64 inc;
};
static pcg_random_state globalPcgRandom = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL };

u32 PcgRandomU32(pcg_random_state *pcg = &globalPcgRandom){
    u64 oldstate = pcg->state;
    // Advance internal state
    pcg->state = oldstate*6364136223846793005ULL + (pcg->inc | 1);
    // Calculate output function (XSH RR), uses old state for max ILP
    u32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    u32 rot = oldstate >> 59u;
    u32 result = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
	return result;
}
// Seed the rng.  Specified in two parts, state initializer and a
// sequence selection constant (a.k.a. stream id)
void PcgRandomSeed(u64 initstate, u64 initseq) {
	auto rng = &globalPcgRandom;
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    PcgRandomU32();
    rng->state += initstate;
    PcgRandomU32();
}


//
// Random
//

//
// Random full value range
//
#include "bi_math.h"
inline u8 RandomU8(){
	u8 result = (u8)PcgRandomU32();
	return result;
}
inline u16 RandomU16(){
	u16 result = (u16)PcgRandomU32();
	return result;
}
inline u32 RandomU32(){
	u32 result = PcgRandomU32();
	return result;
}
inline s32 RandomS32(){
	u32 u = PcgRandomU32();
	s32 result = *(s32 *)&u;
	return result;
}
// Return range: [0, 1] (both inclusive)
inline f32 Random(){
	f32 result = (f32)RandomU32() / 4294967295.f; // 2^32 - 1
	return result;
}
// Return range: [0, 1] (both inclusive)
inline f32 Random01(){
	return Random();
}


//
// Random in a specific range
//

// Return range: [min, max] (both inclusive)
inline f32 RandomRange(f32 min, f32 max){
	f32 t = Random01();
	f32 result = (1.f - t)*min + max*t;
	return result;
}
// Return range: [min, max]
inline s32 RandomRangeS32(s32 min, s32 max){
	u32 range = (u32)(max - min);
	s32 result = min + (s32)MinU32((u32)range, (u32)(Random01()*range + .5f));
	return result;
}
// Return range: [min, max]
inline u32 RandomRangeU32(u32 min, u32 max){
	u32 range = (u32)(max - min);
	s32 result = min + MinU32((u32)range, (u32)(Random01()*range + .5f));
	return result;
}
// Return range: [min, max]
inline u16 RandomRangeU16(u16 min, u16 max){
	u16 result = (u16)RandomRangeU32((u32)min, (u32)max);
	return result;
}
// Return range: [min, max]
inline u8 RandomRangeU8(u8 min, u8 max){
	u8 result = (u8)RandomRangeU32((u32)min, (u32)max);
	return result;
}


//
// Random [0, max]
//

// Random number in range [0, max] (both inclusive)
inline f32 Random(f32 max){
	return Random()*max;
}
// Random number in range [-max, max]
inline f32 RandomBilateral(f32 max){
	return RandomRange(-max, max);
}
// Return range: [0, max]
inline s32 RandomS32(s32 max){
	return RandomRangeS32(0, max);
}
// Random number in range [-max, max]
inline s32 RandomBilateralS32(s32 max){
	return RandomRangeS32(-max, max);
}
// Return range: [0, max]
inline u32 RandomU32(u32 max){
	return RandomRangeU32(0, max);
}
// Return range: [0, max]
inline u16 RandomU16(u16 max){
	return RandomRangeU16(0, max);
}
// Return range: [0, max]
inline u8 RandomU8(u8 min, u8 max){
	return RandomRangeU8(0, max);
}


// If chance <=0 the result will always be false.
// If chance >=1 the result will always be true.
inline b32 RandomChance(f32 chance){
	// 16777216 is 2^24, and is the last consecutive integer perfectly representable by a float.
	u32 randomBits = RandomU32() & 16777215; // 24 random bits.
	f32 randomNumber = (f32)randomBits; // Range: [0, 16777215]
	f32 threshold = chance*16777216.f;  // Range: [0, 16777216]
	if (randomNumber < threshold)
		return true;
	return false;
}

#endif

