// 
// Contains math utilities.
//

// Most functions are named with no suffix in the float version, and with suffix for other types.


#ifndef BI_MATH_H
#define BI_MATH_H

#ifndef PI
	#define PI 3.141592653589793238f
#endif
#define MAX_INTEGER_FLOAT 16777216 // (2^24) The number previous to the first integer that cannot be represented as float.
#define MAX_S32 0x7FFFFFFF  //  2,147,483,648
#define MIN_S32 0x80000000  // -2,147,483,647
#define MAX_U32 0xFFFFFFFFU //  4,294,967,295
#define MAX_F32 340282346638528859811704183484516925440.f // 3.4028234e+38 or 0x7F7FFFFF

#define SQUARE(x) ((x)*(x))
inline f32 Square(f32 x){
	f32 result = x*x;
	return result;
}

inline u32 SimpleHash(u32 a){
	a ^= 2747636419u;
	a *= 2654435769u;
	a ^= a >> 16;
	a *= 2654435769u;
	a ^= a >> 16;
	a *= 2654435769u;
	return a;
}

//
// Some <math.h> wrappers
//
#include <math.h>

inline f32 Cos(f32 x){
	f32 result = cosf(x);
	return result;
}
inline f32 Sin(f32 x){
	f32 result = sinf(x);
	return result;
}
// Arc Cosine. Input range: [-1, 1]. Return range: [0, pi]
inline f32 ACos(f32 x){
	AssertRange(-1.f, x, 1.f);
	f32 result = acosf(x);
	return result;
}
// Arc Sine. Input range: [-1, 1]. Return range: [-pi/2, pi/2]
inline f32 ASin(f32 x){
	AssertRange(-1.f, x, 1.f);
	f32 result = asinf(x);
	return result;
}
// Arc Tangent. Return range: [-pi/2, pi/2]
inline f32 ATan(f32 x){
	f32 result = atanf(x);
	return result;
}
// Arc Tangent of y/x. Return range: [-pi/2, pi/2]
inline f32 ATan2(f32 y, f32 x){
	f32 result = atan2f(y, x);
	return result;
}
inline f32 SquareRoot(f32 x){
	f32 result = sqrtf(x);
	return result;
}


//
// Abs, Frac, Floor, Ceil, Round, Max, Min, Lerp, Clamp...
//

inline f32 Abs(f32 value){
	if (value < 0)
		return -value;
	return value;
}
inline s32 AbsS32(s32 value){
	if (value < 0)
		return -value;
	return value;
}

// Fractional part of value.
inline f32 Frac(f32 x){
	return x - (f32)(s64)x;
}

// Remainder of value/mod. 
f32 FMod(f32 value, f32 mod){
	f32 result = value - (mod * (f32)((s64)(value/mod)));
	return result;
}

inline f32 Floor(f32 x){
	f32 result = (f32)(s32)x;
	if (x < 0 && (result != x)){
		result -= 1.f;
	}
	return result;
}

inline f32 Ceil(f32 x){
	f32 result = (f32)(s32)x;
	if (x > 0 && (result != x)){
		result += 1.f;
	}
	return result;
}

inline f32 Round(f32 x){
	f32 result = Floor(x + .5f);
	return result;
}
inline u32 RoundF32ToU32(f32 x) {
	u32 result = (u32)roundf(x);
	return result;
}
inline s32 RoundF32ToS32(f32 x) {
	s32 result = (s32)roundf(x);
	return result;
}

inline f32 Max(f32 a, f32 b){
	if (a >= b)
		return a;
	return b;
}
inline s32 MaxS32(s32 a, s32 b){
	if (a >= b)
		return a;
	return b;
}
inline u32 MaxU32(u32 a, u32 b){
	if (a >= b)
		return a;
	return b;
}

inline f32 Min(f32 a, f32 b){
	if (a <= b)
		return a;
	return b;
}
inline s32 MinS32(s32 a, s32 b){
	if (a <= b)
		return a;
	return b;
}
inline u32 MinU32(u32 a, u32 b){
	if (a <= b)
		return a;
	return b;
}

inline f32 Lerp(f32 a, f32 b, f32 t){
	f32 result = (1.f - t)*a + b*t;
	return result;
}
// Basically like smoothstep (slope is 1 at .5 and 0 at 0 and 1).
inline f32 SLerp(f32 a, f32 b, f32 t){
	f32 result = Lerp(a, b, 6*t*t*t*t*t - 15*t*t*t*t + 10*t*t*t);
	return result;
}

inline f32 Clamp(f32 value, f32 min, f32 max){
	if (value > max) return max;
	if (value < min) return min;
	return value;
}
inline f32 Clamp01(f32 value){
	return Clamp(value, 0, 1.f);
}
inline s32 ClampS32(s32 value, s32 min, s32 max){
	if (value > max) return max;
	if (value < min) return min;
	return value;
}
inline u32 ClampU32(u32 value, u32 min, u32 max){
	if (value > max) return max;
	if (value < min) return min;
	return value;
}
inline f32 LerpClamp(f32 a, f32 b, f32 t){
	return Lerp(a, b, Clamp01(t));
}

inline f32 Sign(f32 x){
	if (x > 0)  return 1.f;
	else if (x) return -1.f;
	else	    return 0;
}
inline s32 Sign(s32 x){
	if (x > 0)  return 1;
	else if (x) return -1;
	else	    return 0;
}
inline f32 SignNonZero(f32 x){
	if (x < 0)
		return -1.f;
	return 1.f;
}

// - Cubic interpolation.
// - Smoothstep(0)=0    Smoothstep(1)=1    Smoothstep(.5)=.5    ...(.25)=.16    (.75)=.84
f32 Smoothstep(f32 x){
	Assert(x >= 0.0f && x <= 1.0f);
	f32 y = x*x*(3.0f - 2.0f*x);
	return y;
}
f32 SmoothstepClamp(f32 x){
	return Smoothstep(Clamp01(x));
}

// Returns (numerator / denominator) or n.
inline f32 SafeDivideN(f32 numerator, f32 denominator, f32 n){
	return (denominator == 0 ? n : numerator/denominator);
}
// Returns (numerator / denominator) or 0.
inline f32 SafeDivide0(f32 numerator, f32 denominator){
  return SafeDivideN(numerator, denominator, 0);
}
// Returns (numerator / denominator) or 1.
inline f32 SafeDivide1(f32 numerator, f32 denominator){
  return SafeDivideN(numerator, denominator, 1);
}


//
// Angles
//

// NOTE: "Normalized angles" are [-pi, pi]. Most functions work with non-normalized
// angles, and the functions that expect normalized angles make that clear.

// Range: [-pi, pi]
inline f32 NormalizeAngle(f32 a){
	f32 result = FMod(a, 2*PI);
	if (result < 0)
		result += 2*PI;
	if (result > PI)
		result -= 2*PI;
	return result;
}
// Return range: [0, 2*pi]
inline f32 NormalizeAnglePositive(f32 a){
	f32 result = FMod(a, 2*PI);
	if (result < 0)
		result += 2*PI;
	return result;
}

// Returns the angle that must be added to the second angle (in CW direction) to get to
// the first angle (normalized). Return range: [-pi, pi]
f32 AngleDifference(f32 to, f32 from){
	f32 result = NormalizeAngle(to - from);
	return result;
}

// Return range: [-pi, pi]
f32 FlipAngleX(f32 angle){
	angle = NormalizeAnglePositive(angle);
	f32 result = PI - angle;
	return result;
}

// Inputs don't need to be normalized. Can extrapolate. Result is not normalized.
inline f32 LerpAngle(f32 a, f32 b, f32 t){
	f32 diff = AngleDifference(b, a);
	f32 result = a + diff*t;
	return result;
}

// Limits 'a' to range 'limit0' to 'limit1' in CW direction.
// if 'limit0' == 'limit1' the only result will be that limit.
// if  'limit0' == 0 && 'limit1' == 2*PI the result can be any angle.
// Return range: [-pi, pi]
f32 ClampAngle(f32 a, f32 limit0, f32 limit1){
	f32 angle = NormalizeAnglePositive(a);
	if (limit0 != 0 && limit1 != 2*PI){ // Limited range
		limit0 = NormalizeAnglePositive(limit0);
		limit1 = NormalizeAnglePositive(limit1);

		if (limit0 <= limit1){
			if (angle < limit0 || angle > limit1){
				if (AngleDifference(angle, (limit1 + limit0)/2) < 0)
					angle = limit0;
				else
					angle = limit1;
			}
		}else{
			if (angle < limit0 && angle > limit1){
				if (AngleDifference(angle, (limit1 + limit0)/2) < 0)
					angle = limit1;
				else
					angle = limit0;
			}
		}
	}
	if (angle > PI)
		angle -= 2*PI;
	return angle;
}


//
// Move towards
//

f32 MoveTowards(f32 value, f32 target, f32 speed){
	f32 result;
	if (value <= target){
		result = Min(target, value + speed);
	}else{
		result = Max(target, value - speed);
	}
	return result;
}


//
// Mapping
//

// - f(0)=0       f(.5)=1       f(1)=0
// - slope(0)=0   slope(.5)=0   slope(1)=0
inline f32 Map01ToBellSin(f32 x){
	//Assert(x >= 0.0f && x <= 1.0f);
	f32 y = .5f + .5f*Sin(2*PI*(x + .75f));
	return y;
}
// - f(0)=0         f(.5)=.5      f(1)=0
// - slope(0)=inf   slope(.5)=1   slope(1)=inf
inline f32 Map01ToArcSin(f32 x){
	//Assert(x >= 0.0f && x <= 1.0f);
	f32 y = .5f + ASin(2*x - 1.f)/PI;
	return y;
}
// - f(0)=0         f(.5)=1       f(1)=0
// - slope(0)=inf   slope(.5)=0   slope(1)=inf
inline f32 Map01ToSemiCircle(f32 x){
	//Assert(x >= 0.0f && x <= 1.0f);
	f32 y = Sin(ACos((-.5f + x)*2));
	return y;
}
// - min is allowed to be greater than max.
inline f32 MapRangeTo01(f32 x, f32 min, f32 max){
	return (x - min)/(max - min);
}
// - min is allowed to be greater than max.
inline f32 MapRangeToRangeClamp(f32 x, f32 xMin, f32 xMax, f32 yMin, f32 yMax){
	f32 t = Clamp01((x - xMin)/(xMax - xMin));
	f32 y = Lerp(yMin, yMax, t);
	return y;
}



//
// V2
//
struct v2{
	f32 x, y;
};

inline v2 V2(f32 x, f32 y){
	v2 result = {x, y};
	return result;
}

inline v2 V2(f32 xy){
	v2 result = {xy, xy};
	return result;
}


inline v2 operator*(f32 a, v2 b){
	v2 result;

	result.x = a*b.x;
	result.y = a*b.y;

	return result;
}

inline v2 operator*(v2 a, f32 b){
	v2 result;

	result.x = a.x*b;
	result.y = a.y*b;

	return result;
}

inline v2 operator/(v2 a, f32 b){
	v2 result;

	result.x = a.x/b;
	result.y = a.y/b;

	return result;
}

inline v2 operator+(v2 a, v2 b){
	v2 result;
	
	result.x = a.x + b.x;
	result.y = a.y + b.y;

	return result;
}

inline v2 operator-(v2 a, v2 b){
	v2 result;

	result.x = a.x - b.x;
	result.y = a.y - b.y;
	
	return result;
}

inline void operator*=(v2 &a, f32 scalar){
	a = a*scalar;
}

inline void operator/=(v2 &a, f32 scalar){
	a = a/scalar;
}

inline void operator+=(v2 &a, v2 b){
	a = a + b;
}

inline void operator-=(v2 &a, v2 b){
	a = a - b;
}

inline v2 operator-(v2 a){
	v2 result;

	result.x = -a.x;
	result.y = -a.y;

	return result;
}

inline b32 operator==(v2 a, v2 b){
	b32 result = (a.x == b.x) && (a.y == b.y);
	return result;
}

inline b32 operator!=(v2 a, v2 b){
	b32 result = (a.x != b.x) || (a.y != b.y);
	return result;
}


inline f32 Dot(v2 a, v2 b){
	f32 result = a.x*b.x + a.y*b.y;
	return result;
}

inline f32 Cross(v2 a, v2 b){
	f32 result = a.x*b.y - a.y*b.x;
	return result;
}

inline v2 Hadamard(v2 a, v2 b){
	v2 result = {a.x*b.x, a.y*b.y};
	return result;
}

inline f32 Length(v2 a){
	f32 result = SquareRoot(a.x*a.x + a.y*a.y);
	return result;
}
inline f32 LengthSqr(v2 a){
	f32 result = a.x*a.x + a.y*a.y;
	return result;
}


inline b32 PointInRectangle(v2 p, v2 r0, v2 r1){
	if (p.x < r0.x || p.x > r1.x || p.y < r0.y || p.y > r1.y)
		return false;
	return true;
}


//
// V2 angle and direction stuff
//

// - Return range: [-PI, PI]
// - Input can be {0,0}.
inline f32 AngleOf(v2 a){
	f32 result = 0;
	if (a.x && a.y)
		result = ATan2(a.y, a.x);
	return result;
}

inline v2 V2LengthDir(f32 length, f32 direction){
	v2 result = {Cos(direction)*length, Sin(direction)*length};
	return result;
}

// - Return range: [-pi, pi].
// - Input can be {0,0}.
inline f32 AngleBetween(v2 from, v2 to){
	f32 dot = Dot(from, to);
	f32 cross = Cross(from, to);
	f32 angle = AngleOf(V2(dot, cross));
	return angle;
}

inline v2 RotateV2(v2 v, f32 angle){
	f32 sin = Sin(angle), cos = Cos(angle);
	v2 result = {v.x*cos - v.y*sin, v.x*sin + v.y*cos};
	return result;
}

inline v2 Rotate90Degrees(v2 v){
	v2 result = {-v.y, v.x};
	return result;
}
inline v2 RotateMinus90Degrees(v2 v){
	v2 result = {v.y, -v.x};
	return result;
}



//
// Integer V2
//
struct v2s{
	s32 x;
	s32 y;
};

inline v2s V2S(s32 x, s32 y){
	v2s result = {x, y};
	return result;
}

inline v2s V2S(s32 xy){
	v2s result = {xy, xy};
	return result;
}

inline v2s V2S(v2 a){
	v2s result = {(s32)a.x, (s32)a.y};
	return result;
}

inline v2 V2(v2s a){
	v2 result = {(f32)a.x, (f32)a.y};
	return result;
}


inline v2s operator*(s32 a, v2s b){
	v2s result = {a*b.x, a*b.y};
	return result;
}

inline v2s operator*(v2s a, s32 b){
	v2s result = {a.x*b, a.y*b};
	return result;
}

inline v2s operator/(v2s a, s32 b){
	v2s result = {a.x/b, a.y/b};
	return result;
}

inline v2s &operator*=(v2s &a, s32 b){
	a = a*b;
	return a;
}

inline v2s &operator/=(v2s &a, s32 b){
	a = a/b;
	return a;
}

inline v2s operator+(v2s a, v2s b){
	v2s result = {a.x + b.x, a.y + b.y};
	return result;
}

inline v2s &operator+=(v2s &a, v2s b){
	a = a + b;
	return a;
}

inline v2s operator-(v2s a, v2s b){
	v2s result = {a.x - b.x, a.y - b.y};
	return result;
}

inline v2s &operator-=(v2s &a, v2s b){
	a = a - b;
	return a;
}

inline v2s operator-(v2s a){
	v2s result = {-a.x, -a.y};
	return result;
}

inline b32 operator==(v2s a, v2s b){
	b32 result = (a.x == b.x) && (a.y == b.y);
	return result;
}

inline b32 operator!=(v2s a, v2s b){
	b32 result = (a.x != b.x) || (a.y != b.y);
	return result;
}


inline s32 LengthSqr(v2s a){
	s32 result = a.x*a.x + a.y*a.y;
	return result;
}

inline s32 DotV2S(v2s a, v2s b){
	s32 result = a.x*b.x + a.y*b.y;
	return result;
}
inline s32 CrossV2S(v2s a, v2s b){
	s32 result = a.x*b.y - a.y*b.x;
	return result;
}
inline v2s HadamardV2S(v2s a, v2s b){
	v2s result = {a.x*a.x, a.y*a.y};
	return result;
}


//
// V2 and V2S functions
//

inline v2 Clamp01V2(v2 v){
  v2 result;
  result.x = Clamp01(v.x);
  result.y = Clamp01(v.y);
  return result;
}
inline v2 ClampV2(v2 v, v2 min, v2 max){
	v2 result = {Clamp(v.x, min.x, max.x), Clamp(v.y, min.y, max.y)};
	return result;
}
inline v2s ClampV2S(v2s v, v2s min, v2s max){
	v2s result = {ClampS32(v.x, min.x, max.x), ClampS32(v.y, min.y, max.y)};
	return result;
}

inline v2 LerpV2(v2 a, v2 b, f32 t){
	return (1.0f-t)*a + t*b;
}

inline v2s LerpV2S(v2s a, v2s b, f32 t){
	v2s result = a + V2S(V2(b - a)*t);
	return result;
}

inline v2 RoundV2(v2 v){
	v2 result = { Round(v.x), Round(v.y) };
	return result;
}

inline v2 MaxV2(v2 a, v2 b){
	v2 result = { Max(a.x, b.x), Max(a.y, b.y) };
	return result;
}
inline v2 MinV2(v2 a, v2 b){
	v2 result = { Min(a.x, b.x), Min(a.y, b.y) };
	return result;
}

inline v2s MaxV2S(v2s a, v2s b){
	v2s result = { MaxS32(a.x, b.x), MaxS32(a.y, b.y) };
	return result;
}
inline v2s MinV2S(v2s a, v2s b){
	v2s result = { MinS32(a.x, b.x), MinS32(a.y, b.y) };
	return result;
}
inline v2 AbsV2(v2 a){
	v2 result = {Abs(a.x), Abs(a.y)};
	return result;
}

inline v2 LimitLengthV2(v2 a, f32 maxLength){
	f32 lenSqr = LengthSqr(a);
	if (lenSqr > maxLength*maxLength){
		f32 dir = AngleOf(a);
		a = V2LengthDir(maxLength, dir);
	}
	return a;
}

inline v2 Normalize(v2 a){
	v2 result = V2LengthDir(1.f, AngleOf(a));
	return result;
}

inline v2 EdgeGetNormal(v2 edge){
	v2 normalized = Normalize(edge);
	v2 result = {normalized.y, -normalized.x};
	return result;
}


inline b32 CircleInRectangle(v2 c, f32 r, v2 rectPos, v2 rectDim){
	v2 p = AbsV2(c - (rectPos + rectDim/2));

	b32 result;
    if (p.x > rectDim.x/2 + r || p.y > rectDim.y/2 + r){ // Right/Top quadrants
		result = false;
	}else if (p.x <= rectDim.x/2 || p.y <= rectDim.y/2){ // Bottom-Left quadrant
		result = true; 
	}else{
		f32 cornerDisSqr = LengthSqr(p - rectDim/2);
		result = (cornerDisSqr <= r*r);
	}
	return result;
}

inline b32 SegmentsIntersect(v2 a0, v2 a1, v2 b0, v2 b1){
	f32 b0Proj = Cross(b0 - a0, a1 - a0);
	f32 b1Proj = Cross(b1 - a0, a1 - a0);
	f32 a0Proj = Cross(a0 - b0, b1 - b0);
	f32 a1Proj = Cross(a1 - b0, b1 - b0);
	b32 result = false;
	if (Sign(b0Proj) != Sign(b1Proj) && Sign(a0Proj) != Sign(a1Proj)){
		result = true;
	}
	return result;
}



//
// V4
//
union v4{
	struct{
		f32 r, g, b, a;
	};
	struct{
		f32 x, y, z, w;
	};
	f32 asArray[4];
};

inline v4 V4(f32 r, f32 g, f32 b, f32 a = 1.f){
	v4 result = {r, g, b, a};
	return result;
}


inline v4 operator+(v4 a, v4 b){
	v4 result = {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
	return result;
}

inline v4 &operator+=(v4 &a, v4 b){
  a = a + b;
  return a;
}

inline v4 operator-(v4 a){
	v4 result = {-a.x, -a.y, -a.z, -a.w};
	return result;
}

inline v4 operator-(v4 a, v4 b){
	v4 result = {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
	return result;
}

inline v4 &operator-=(v4 &a, v4 b){
  a = a - b;
  return a;
}

inline v4 operator*(f32 a, v4 b){
  v4 result = {a*b.x, a*b.y, a*b.z, a*b.w};
  return result;
}

inline v4 operator*(v4 a, f32 b){
  v4 result = b * a;
  return result;
}

inline v4 &operator*=(v4 &a, f32 b){
  a = b * a;
  return a;
}

inline b32 operator==(v4 a, v4 b){
	b32 result = (a.x == b.x) && (a.y == b.y) && (a.z == b.z) && (a.w == b.w);
	return result;
}

inline b32 operator!=(v4 a, v4 b){
	b32 result = (a.x != b.x) || (a.y != b.y) || (a.z != b.z) || (a.w != b.w);
	return result;
}


inline v4 Hadamard(v4 a, v4 b){
  v4 result = { a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w };
  return result;
}

inline v4 ClampV4(v4 v, f32 min, f32 max){
	v4 result = {Clamp(v.x,min,max), Clamp(v.y,min,max), Clamp(v.z,min,max), Clamp(v.w,min,max)};
	return result;
}

inline v4 Clamp01V4(v4 v){
	v4 result = ClampV4(v, 0.0f, 1.0f);
	return result;
}

inline v4 LerpV4(v4 a, v4 b, f32 t){
	v4 result = {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t), Lerp(a.w, b.w, t)};
	return result;
}

// Colors
inline v4 V4_Red    (f32 alpha = 1.f){ return V4(1.f, 0, 0, alpha); }
inline v4 V4_Green  (f32 alpha = 1.f){ return V4(0, 1.f, 0, alpha); }
inline v4 V4_Blue   (f32 alpha = 1.f){ return V4(0, 0, 1.f, alpha); }
inline v4 V4_Yellow (f32 alpha = 1.f){ return V4(1.f, 1.f, 0, alpha); }
inline v4 V4_Cyan   (f32 alpha = 1.f){ return V4(0, 1.f, 1.f, alpha); }
inline v4 V4_Magenta(f32 alpha = 1.f){ return V4(1.f, 0, 1.f, alpha); }

inline v4 V4_White(f32 alpha = 1.f){ return V4(1.f, 1.f, 1.f, alpha); }
inline v4 V4_Black(f32 alpha = 1.f){ return V4(0, 0, 0, alpha); }
inline v4 V4_Grey(f32 value = .5f, f32 alpha = 1.f){ return V4(value, value, value, alpha); }



// Return range: [0, 1]
// Hue: 0=red .166=yellow .333=green .5=cyan .666=blue .833=magenta
v4 ColorFromHSV(f32 h, f32 s, f32 v, f32 a = 1.0f){
	Assert(h >= 0.0f && h <= 1.0f && s >= 0.0f && s <= 1.0f && v >= 0.0f && v <= 1.0f);

	h *= 360.0f;

    f32 c = s*v;
    f32 x = c*(1.0f - Abs(FMod(h/60.0f, 2.0f) - 1.0f));
    f32 m = v - c;
    v4 result;
    if(h < 60.0f){
		result = V4(c, x, 0.0f, a);
    }else if(h < 120.0f){
		result = V4(x, c, 0.0f, a);
    }else if(h < 180.0f){
		result = V4(0.0f, c, x, a);
    }else if(h < 240.0f){
		result = V4(0.0f, x, c, a);
    }else if(h < 300.0f){
		result = V4(x, 0.0f, c, a);
    }else{
		result = V4(c, 0.0f, x, a);
    }
	
	result += V4(m, m, m, 0.0f);
	return result;
}



#endif