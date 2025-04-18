// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Collision/Functions.h>
#include <AnKi/Collision/Sphere.h>

namespace anki {

void extractClipPlane(const Mat4& mvp, FrustumPlaneType id, Plane& plane)
{
	// This code extracts the planes assuming the projection matrices are DX-like right-handed (eg D3DXMatrixPerspectiveFovRH)
	// See "Fast Extraction of Viewing Frustum Planes from the WorldView-Projection Matrix" paper for more info

#define ANKI_CASE(i, a, op, b) \
	case i: \
	{ \
		const Vec4 planeEqationCoefs = mvp.getRow(a) op mvp.getRow(b); \
		const Vec4 n = planeEqationCoefs.xyz0(); \
		const F32 len = n.length(); \
		plane = Plane(n / len, -planeEqationCoefs.w() / len); \
		break; \
	}

	switch(id)
	{
	case FrustumPlaneType::kNear:
	{
		const Vec4 planeEqationCoefs = mvp.getRow(2);
		const Vec4 n = planeEqationCoefs.xyz0();
		const F32 len = n.length();
		plane = Plane(n / len, -planeEqationCoefs.w() / len);
		break;
	}
		ANKI_CASE(FrustumPlaneType::kFar, 3, -, 2)
		ANKI_CASE(FrustumPlaneType::kLeft, 3, +, 0)
		ANKI_CASE(FrustumPlaneType::kRight, 3, -, 0)
		ANKI_CASE(FrustumPlaneType::kTop, 3, -, 1)
		ANKI_CASE(FrustumPlaneType::kBottom, 3, +, 1)
	default:
		ANKI_ASSERT(0);
	}

#undef ANKI_CASE
}

void extractClipPlanes(const Mat4& mvp, Array<Plane, 6>& planes)
{
	for(U i = 0; i < 6; ++i)
	{
		extractClipPlane(mvp, static_cast<FrustumPlaneType>(i), planes[i]);
	}
}

void computeEdgesOfFrustum(F32 far, F32 fovX, F32 fovY, Vec3 points[4])
{
	// This came from unprojecting. It works, don't touch it
	const F32 x = far * tan(fovY / 2.0f) * fovX / fovY;
	const F32 y = tan(fovY / 2.0f) * far;
	const F32 z = -far;

	points[0] = Vec3(x, y, z); // top right
	points[1] = Vec3(-x, y, z); // top left
	points[2] = Vec3(-x, -y, z); // bot left
	points[3] = Vec3(x, -y, z); // bot right
}

static Vec4 computeBoundingSphere2(const Vec3 O, const Vec3 A)
{
	const Vec3 a = A - O;

	const Vec3 o = 0.5f * a;

	const F32 radius = o.length() + kEpsilonf;
	const Vec3 center = O + o;

	return Vec4(center, radius);
}

static Vec4 computeBoundingSphere3(const Vec3 O, const Vec3 A, const Vec3 B)
{
	const Vec3 a = A - O;
	const Vec3 b = B - O;

	const Vec3 acrossb = a.cross(b);
	const F32 denominator = 2.0f * (acrossb.dot(acrossb));

	if(denominator == 0.0f) [[unlikely]]
	{
		// A pair in A,B,O are the same point or they are in the same line

		if(a.lengthSquared() > b.lengthSquared())
		{
			return computeBoundingSphere2(O, A);
		}
		else
		{
			return computeBoundingSphere2(O, B);
		}
	}

	Vec3 o = b.dot(b) * acrossb.cross(a);
	o += a.dot(a) * b.cross(acrossb);
	o /= denominator;

	const F32 radius = o.length() + kEpsilonf;
	const Vec3 center = O + o;

	return Vec4(center, radius);
}

Vec4 computeBoundingSphereRecursive(WeakArray<const Vec3*> pPoints, U32 begin, U32 p, U32 b)
{
	Vec4 sphere;

	switch(b)
	{
	case 0:
		sphere = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
		break;
	case 1:
		sphere = Vec4(*pPoints[begin - 1], kEpsilonf);
		break;
	case 2:
		sphere = computeBoundingSphere2(*pPoints[begin - 1], *pPoints[begin - 2]);
		break;
	case 3:
		sphere = computeBoundingSphere3(*pPoints[begin - 1], *pPoints[begin - 2], *pPoints[begin - 3]);
		return sphere;
		break;
#if 0
	// There is this case as well but it fails if points are coplanar so avoid it
	case 4:
	{
		const Vec3 O = *pPoints[begin - 1];
		const Vec3 A = *pPoints[begin - 2];
		const Vec3 B = *pPoints[begin - 3];
		const Vec3 C = *pPoints[begin - 4];

		const Vec3 a = A - O;
		const Vec3 b = B - O;
		const Vec3 c = C - O;

		auto compDet = [](F32 m11, F32 m12, F32 m13, F32 m21, F32 m22, F32 m23, F32 m31, F32 m32, F32 m33) {
			return m11 * (m22 * m33 - m32 * m23) - m21 * (m12 * m33 - m32 * m13) + m31 * (m12 * m23 - m22 * m13);
		};

		const F32 denominator = 2.0f * compDet(a.x(), a.y(), a.z(), b.x(), b.y(), b.z(), c.x(), c.y(), c.z());

		Vec3 o = c.dot(c) * a.cross(b);
		o += b.dot(b) * c.cross(a);
		o += a.dot(a) * b.cross(c);
		o /= denominator;

		const F32 radius = o.getLength() + kEpsilonf;
		const Vec3 center = O + o;

		sphere = Vec4(center, radius);

		return sphere;
	}
#endif
	default:
		ANKI_ASSERT(0);
	}

	for(U32 i = 0; i < p; i++)
	{
		const F32 distSq = (sphere.xyz() - *pPoints[begin + i]).lengthSquared();
		const F32 radiusSq = sphere.w() * sphere.w();

		if(distSq > radiusSq)
		{
			for(U32 j = i; j > 0; j--)
			{
				const Vec3* T = pPoints[begin + j];
				pPoints[begin + j] = pPoints[begin + j - 1];
				pPoints[begin + j - 1] = T;
			}

			sphere = computeBoundingSphereRecursive(pPoints, begin + 1, i, b + 1);
		}
	}

	return sphere;
}

Sphere computeBoundingSphere(const Vec3* firstPoint, U32 pointCount, PtrSize stride)
{
	ANKI_ASSERT(firstPoint);
	ANKI_ASSERT(pointCount >= 3);
	ANKI_ASSERT(stride >= sizeof(Vec3));

	DynamicArray<const Vec3*> pPointsDyn;
	Array<const Vec3*, 8> pPointsArr;
	WeakArray<const Vec3*> pPoints;

	if(pointCount > pPointsArr.getSize())
	{
		pPointsDyn.resize(pointCount);
		pPoints = pPointsDyn;
	}
	else
	{
		pPoints = {&pPointsArr[0], pointCount};
	}

	for(U32 i = 0; i < pointCount; ++i)
	{
		pPoints[i] = reinterpret_cast<const Vec3*>(ptrToNumber(firstPoint) + i * stride);
	}

	const Vec4 sphere = computeBoundingSphereRecursive(pPoints, 0, pPoints.getSize(), 0);

	return Sphere(sphere.xyz(), sphere.w());
}

Aabb computeBoundingAabb(const Vec3* firstPoint, U32 pointCount, PtrSize stride)
{
	ANKI_ASSERT(firstPoint);
	ANKI_ASSERT(pointCount >= 3);
	ANKI_ASSERT(stride >= sizeof(Vec3));

	Vec3 min(kMaxF32);
	Vec3 max(kMinF32);

	for(U32 i = 0; i < pointCount; ++i)
	{
		const Vec3& point = *reinterpret_cast<const Vec3*>(ptrToNumber(firstPoint) + i * stride);

		min = min.min(point);
		max = max.max(point);
	}

	max += 10.0f * kEpsilonf;

	return Aabb(min, max);
}

} // end namespace anki
