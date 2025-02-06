// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Math/Common.h>
#include <AnKi/Math/Vec.h>

namespace anki {

/// @addtogroup math
/// @{

/// Matrix type.
/// @tparam T The scalar type. Eg float.
/// @tparam kTRowCount The number of rows.
/// @tparam kTColumnCount The number of columns.
template<typename T, U kTRowCount, U kTColumnCount>
class alignas(MathSimd<T, kTColumnCount>::kAlignment) TMat
{
public:
	using Scalar = T;
	using Simd = typename MathSimd<T, kTColumnCount>::Type;

#if ANKI_COMPILER_GCC_COMPATIBLE
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
	using SimdArray = Array<Simd, kTRowCount>;
#if ANKI_COMPILER_GCC_COMPATIBLE
#	pragma GCC diagnostic pop
#endif

	using RowVec = TVec<T, kTColumnCount>;
	using ColumnVec = TVec<T, kTRowCount>;

	static constexpr U kRowCount = kTRowCount; ///< Number of rows
	static constexpr U kColumnCount = kTColumnCount; ///< Number of columns
	static constexpr U kSize = kTRowCount * kTColumnCount; ///< Number of total elements

	static constexpr Bool kIsSquare = kTColumnCount == kTRowCount;
	static constexpr Bool kHasSimd = kTColumnCount == 4 && std::is_same<T, F32>::value && ANKI_ENABLE_SIMD;
	static constexpr Bool kIs4x4Simd = kTRowCount == 4 && kTColumnCount == 4 && std::is_same<T, F32>::value && ANKI_ENABLE_SIMD;
	static constexpr Bool kIs3x4Simd = kTRowCount == 3 && kTColumnCount == 4 && std::is_same<T, F32>::value && ANKI_ENABLE_SIMD;

	/// @name Constructors
	/// @{
	constexpr TMat()
		: TMat(T(0))
	{
	}

	/// Copy.
	constexpr TMat(const TMat& b)
	{
		for(U i = 0; i < kRowCount; i++)
		{
			m_rows[i] = b.m_rows[i];
		}
	}

	explicit constexpr TMat(const T f)
	{
		for(U i = 0; i < kRowCount; i++)
		{
			m_rows[i] = RowVec(f);
		}
	}

	explicit constexpr TMat(const T arr[])
	{
		for(U i = 0; i < N; i++)
		{
			m_arr1[i] = arr[i];
		}
	}

	// 3x3 specific constructors

	constexpr TMat(T m00, T m01, T m02, T m10, T m11, T m12, T m20, T m21, T m22) requires(kSize == 9)
	{
		auto& m = *this;
		m(0, 0) = m00;
		m(0, 1) = m01;
		m(0, 2) = m02;
		m(1, 0) = m10;
		m(1, 1) = m11;
		m(1, 2) = m12;
		m(2, 0) = m20;
		m(2, 1) = m21;
		m(2, 2) = m22;
	}

	explicit constexpr TMat(const TQuat<T>& q) requires(kSize == 9)
	{
		setRotationPart(q);
	}

	explicit constexpr TMat(const TEuler<T>& e) requires(kSize == 9)
	{
		setRotationPart(e);
	}

	explicit constexpr TMat(const TAxisang<T>& axisang) requires(kSize == 9)
	{
		setRotationPart(axisang);
	}

	// 4x4 specific constructors

	constexpr TMat(T m00, T m01, T m02, T m03, T m10, T m11, T m12, T m13, T m20, T m21, T m22, T m23, T m30, T m31, T m32,
				   T m33) requires(kSize == 16)
	{
		auto& m = *this;
		m(0, 0) = m00;
		m(0, 1) = m01;
		m(0, 2) = m02;
		m(0, 3) = m03;
		m(1, 0) = m10;
		m(1, 1) = m11;
		m(1, 2) = m12;
		m(1, 3) = m13;
		m(2, 0) = m20;
		m(2, 1) = m21;
		m(2, 2) = m22;
		m(2, 3) = m23;
		m(3, 0) = m30;
		m(3, 1) = m31;
		m(3, 2) = m32;
		m(3, 3) = m33;
	}

	constexpr TMat(const TVec<T, 3>& translation, const TMat<T, 3, 3>& rotation, const TVec<T, 3>& scale = TVec<T, 3>(T(1))) requires(kSize == 16)
	{
		if(scale == TVec<T, 3>(T(1)))
		{
			setRotationPart(rotation);
		}
		else
		{
			const auto a = rotation.getColumn(0) * scale.x();
			const auto b = rotation.getColumn(1) * scale.y();
			const auto c = rotation.getColumn(2) * scale.z();
			TMat<T, 3, 3> rot;
			rot.setColumns(a, b, c);
			setRotationPart(rot);
		}

		setTranslationPart(translation);

		auto& m = *this;
		m(3, 0) = m(3, 1) = m(3, 2) = T(0);
		m(3, 3) = T(1);
	}

	explicit constexpr TMat(const TTransform<T>& t) requires(kSize == 16)
		: TMat(t.getOrigin().xyz(), t.getRotation().getRotationPart(), t.getScale().xyz())
	{
	}

	/// Set a 4x4 matrix using a 3x4 for the first 3 rows and a vec4 for the 4rth row.
	explicit constexpr TMat(const TMat<T, 3, 4>& m3, const TVec<T, 4>& row3) requires(kSize == 16)
	{
		setRow(0, m3.getRow(0));
		setRow(1, m3.getRow(1));
		setRow(2, m3.getRow(2));
		setRow(3, row3);
	}

	// 3x4 specific constructors

	constexpr TMat(T m00, T m01, T m02, T m03, T m10, T m11, T m12, T m13, T m20, T m21, T m22, T m23) requires(kSize == 12)
	{
		auto& m = *this;
		m(0, 0) = m00;
		m(0, 1) = m01;
		m(0, 2) = m02;
		m(0, 3) = m03;
		m(1, 0) = m10;
		m(1, 1) = m11;
		m(1, 2) = m12;
		m(1, 3) = m13;
		m(2, 0) = m20;
		m(2, 1) = m21;
		m(2, 2) = m22;
		m(2, 3) = m23;
	}

	explicit constexpr TMat(const TMat<T, 4, 4>& m4) requires(kSize == 12)
	{
		ANKI_ASSERT(m4(3, 0) == T(0) && m4(3, 1) == T(0) && m4(3, 2) == T(0) && m4(3, 3) == T(1));
		m_rows[0] = m4.getRow(0);
		m_rows[1] = m4.getRow(1);
		m_rows[2] = m4.getRow(2);
	}

	explicit constexpr TMat(const TVec<T, 3>& translation, const TMat<T, 3, 3>& rotation,
							const TVec<T, 3>& scale = TVec<T, 3>(T(1))) requires(kSize == 12)
	{
		if(scale == TVec<T, 3>(T(1)))
		{
			setRotationPart(rotation);
		}
		else
		{
			const auto a = rotation.getColumn(0) * scale.x();
			const auto b = rotation.getColumn(1) * scale.y();
			const auto c = rotation.getColumn(2) * scale.z();
			setColumns(a, b, c);
		}

		setTranslationPart(translation);
	}

	explicit constexpr TMat(const TVec<T, 3>& translation, const TQuat<T>& q, const TVec<T, 3>& scale = TVec<T, 3>(T(1))) requires(kSize == 12)
		: TMat(translation, TMat<T, 3, 3>(q), scale)
	{
	}

	explicit constexpr TMat(const TVec<T, 3>& translation, const TEuler<T>& b, const TVec<T, 3>& scale = TVec<T, 3>(T(1))) requires(kSize == 12)
		: TMat(translation, TMat<T, 3, 3>(b), scale)
	{
	}

	explicit constexpr TMat(const TVec<T, 3>& translation, const TAxisang<T>& b, const TVec<T, 3>& scale = TVec<T, 3>(T(1))) requires(kSize == 12)
		: TMat(translation, TMat<T, 3, 3>(b), scale)
	{
	}

	explicit constexpr TMat(const TTransform<T>& t) requires(kSize == 12)
		: TMat(t.getOrigin().xyz(), t.getRotation().getRotationPart(), t.getScale().xyz())
	{
	}
	/// @}

	/// @name Accessors
	/// @{
	T& operator()(const U j, const U i)
	{
		return m_arr2[j][i];
	}

	T operator()(const U j, const U i) const
	{
		return m_arr2[j][i];
	}

	T& operator[](const U n)
	{
		return m_arr1[n];
	}

	T operator[](const U n) const
	{
		return m_arr1[n];
	}
	/// @}

	/// @name Operators with same type
	/// @{

	/// Copy.
	TMat& operator=(const TMat& b)
	{
		for(U i = 0; i < kRowCount; ++i)
		{
			m_rows[i] = b.m_rows[i];
		}
		return *this;
	}

	TMat operator+(const TMat& b) const
	{
		TMat c;
		for(U i = 0; i < kRowCount; ++i)
		{
			c.m_rows[i] = m_rows[i] + b.m_rows[i];
		}
		return c;
	}

	TMat& operator+=(const TMat& b)
	{
		for(U i = 0; i < kRowCount; ++i)
		{
			m_rows[i] += b.m_rows[i];
		}
		return *this;
	}

	TMat operator-(const TMat& b) const
	{
		TMat c;
		for(U i = 0; i < kRowCount; ++i)
		{
			c.m_rows[i] = m_rows[i] - b.m_rows[i];
		}
		return c;
	}

	TMat& operator-=(const TMat& b)
	{
		for(U i = 0; i < kRowCount; ++i)
		{
			m_rows[i] -= b.m_rows[i];
		}
		return *this;
	}

	TMat operator*(const TMat& b) const requires(kIsSquare && !kIs4x4Simd)
	{
		TMat out;
		const TMat& a = *this;
		for(U j = 0; j < kTRowCount; j++)
		{
			for(U i = 0; i < kTColumnCount; i++)
			{
				out(j, i) = T(0);
				for(U k = 0; k < kTColumnCount; k++)
				{
					out(j, i) += a(j, k) * b(k, i);
				}
			}
		}
		return out;
	}

#if ANKI_ENABLE_SIMD
	TMat operator*(const TMat& b) const requires(kIs4x4Simd)
	{
		TMat out;
		const auto& m = *this;
		for(U i = 0; i < 4; i++)
		{
#	if ANKI_SIMD_SSE
			__m128 t1, t2;

			t1 = _mm_set1_ps(m(i, 0));
			t2 = _mm_mul_ps(b.m_simd[0], t1);
			t1 = _mm_set1_ps(m(i, 1));
			t2 = _mm_add_ps(_mm_mul_ps(b.m_simd[1], t1), t2);
			t1 = _mm_set1_ps(m(i, 2));
			t2 = _mm_add_ps(_mm_mul_ps(b.m_simd[2], t1), t2);
			t1 = _mm_set1_ps(m(i, 3));
			t2 = _mm_add_ps(_mm_mul_ps(b.m_simd[3], t1), t2);

			out.m_simd[i] = t2;
#	else
			float32x4_t t1, t2;

			t1 = vmovq_n_f32(m(i, 0));
			t2 = b.m_simd[0] * t1;
			t1 = vmovq_n_f32(m(i, 1));
			t2 = b.m_simd[1] * t1 + t2;
			t1 = vmovq_n_f32(m(i, 2));
			t2 = b.m_simd[2] * t1 + t2;
			t1 = vmovq_n_f32(m(i, 3));
			t2 = b.m_simd[3] * t1 + t2;

			out.m_simd[i] = t2;
#	endif
		}

		return out;
	}
#endif

	TMat& operator*=(const TMat& b)
	{
		(*this) = (*this) * b;
		return *this;
	}

	Bool operator==(const TMat& b) const
	{
		for(U i = 0; i < N; i++)
		{
			if(!isZero<T>(m_arr1[i] - b.m_arr1[i]))
			{
				return false;
			}
		}
		return true;
	}

	Bool operator!=(const TMat& b) const
	{
		for(U i = 0; i < N; i++)
		{
			if(!isZero<T>(m_arr1[i] - b.m_arr1[i]))
			{
				return true;
			}
		}
		return false;
	}
	/// @}

	/// @name Operators with T
	/// @{
	TMat operator+(const T f) const
	{
		TMat out;
		for(U i = 0; i < kRowCount; ++i)
		{
			out.m_rows[i] = m_rows[i] + f;
		}
		return out;
	}

	TMat& operator+=(const T f)
	{
		for(U i = 0; i < kRowCount; ++i)
		{
			m_rows[i] += f;
		}
		return *this;
	}

	TMat operator-(const T f) const
	{
		TMat out;
		for(U i = 0; i < kRowCount; ++i)
		{
			out.m_rows[i] = m_rows[i] - f;
		}
		return out;
	}

	TMat& operator-=(const T f)
	{
		for(U i = 0; i < kRowCount; ++i)
		{
			m_rows[i] -= f;
		}
		return *this;
	}

	TMat operator*(const T f) const
	{
		TMat out;
		for(U i = 0; i < kRowCount; ++i)
		{
			out.m_rows[i] = m_rows[i] * f;
		}
		return out;
	}

	TMat& operator*=(const T f)
	{
		for(U i = 0; i < kRowCount; ++i)
		{
			m_rows[i] *= f;
		}
		return *this;
	}

	TMat operator/(const T f) const
	{
		ANKI_ASSERT(f != T(0));
		TMat out;
		for(U i = 0; i < kRowCount; ++i)
		{
			out.m_rows[i] = m_rows[i] / f;
		}
		return out;
	}

	TMat& operator/=(const T f)
	{
		ANKI_ASSERT(f != T(0));
		for(U i = 0; i < kRowCount; ++i)
		{
			m_rows[i] /= f;
		}
		return *this;
	}
	/// @}

	/// @name Operators with other types
	/// @{
	ColumnVec operator*(const RowVec& v) const requires(!kHasSimd)
	{
		const TMat& m = *this;
		ColumnVec out;
		for(U j = 0; j < kTRowCount; j++)
		{
			T sum = T(0);
			for(U i = 0; i < kTColumnCount; i++)
			{
				sum += m(j, i) * v[i];
			}
			out[j] = sum;
		}
		return out;
	}

#if ANKI_SIMD_SSE
	ColumnVec operator*(const RowVec& v) const requires(kIs4x4Simd)
	{
		__m128 a = _mm_mul_ps(m_simd[0], v.getSimd());
		__m128 b = _mm_mul_ps(m_simd[1], v.getSimd());
		__m128 c = _mm_mul_ps(m_simd[2], v.getSimd());
		__m128 d = _mm_mul_ps(m_simd[3], v.getSimd());

		a = _mm_hadd_ps(a, b);
		c = _mm_hadd_ps(c, d);

		return RowVec(_mm_hadd_ps(a, c));
	}

	ColumnVec operator*(const RowVec& v) const requires(kIs3x4Simd)
	{
		__m128 a = _mm_mul_ps(m_simd[0], v.getSimd());
		__m128 b = _mm_mul_ps(m_simd[1], v.getSimd());
		__m128 c = _mm_mul_ps(m_simd[2], v.getSimd());

		a = _mm_hadd_ps(a, b);
		const RowVec d(_mm_hadd_ps(a, c));

		return ColumnVec(d[0], d[1], d[2] + d[3]);
	}

#else

	ColumnVec operator*(const RowVec& v) const requires(kHasSimd)
	{
		ColumnVec out;
		for(U i = 0; i < kTRowCount; i++)
		{
			out[i] = RowVec(m_simd[i]).dot(v);
		}
		return out;
	}
#endif
	/// @}

	/// @name Other
	/// @{
	void setRow(const U j, const RowVec& v)
	{
		m_rows[j] = v;
	}

	void setRows(const RowVec& a, const RowVec& b, const RowVec& c)
	{
		setRow(0, a);
		setRow(1, b);
		setRow(2, c);
	}

	void setRows(const RowVec& a, const RowVec& b, const RowVec& c, const RowVec& d) requires(kTRowCount > 3)
	{
		setRows(a, b, c);
		setRow(3, d);
	}

	const RowVec& getRow(const U j) const
	{
		return m_rows[j];
	}

	void getRows(RowVec& a, RowVec& b, RowVec& c) const
	{
		a = getRow(0);
		b = getRow(1);
		c = getRow(2);
	}

	void getRows(RowVec& a, RowVec& b, RowVec& c, RowVec& d) const requires(kTRowCount > 3)
	{
		getRows(a, b, c);
		d = getRow(3);
	}

	void setColumn(const U i, const ColumnVec& v)
	{
		for(U j = 0; j < kTRowCount; j++)
		{
			m_arr2[j][i] = v[j];
		}
	}

	void setColumns(const ColumnVec& a, const ColumnVec& b, const ColumnVec& c)
	{
		setColumn(0, a);
		setColumn(1, b);
		setColumn(2, c);
	}

	void setColumns(const ColumnVec& a, const ColumnVec& b, const ColumnVec& c, const ColumnVec& d) requires(kTColumnCount > 3)
	{
		setColumns(a, b, c);
		setColumn(3, d);
	}

	ColumnVec getColumn(const U i) const
	{
		ColumnVec out;
		for(U j = 0; j < kTRowCount; j++)
		{
			out[j] = m_arr2[j][i];
		}
		return out;
	}

	void getColumns(ColumnVec& a, ColumnVec& b, ColumnVec& c) const
	{
		a = getColumn(0);
		b = getColumn(1);
		c = getColumn(2);
	}

	void getColumns(ColumnVec& a, ColumnVec& b, ColumnVec& c, ColumnVec& d) const requires(kTColumnCount > 3)
	{
		getColumns(a, b, c);
		d = getColumn(3);
	}

	/// Get 1st column
	ColumnVec getXAxis() const
	{
		return getColumn(0);
	}

	/// Get 2nd column
	ColumnVec getYAxis() const
	{
		return getColumn(1);
	}

	/// Get 3rd column
	ColumnVec getZAxis() const
	{
		return getColumn(2);
	}

	/// Set 1st column
	void setXAxis(const ColumnVec& v)
	{
		setColumn(0, v);
	}

	/// Set 2nd column
	void setYAxis(const ColumnVec& v)
	{
		setColumn(1, v);
	}

	/// Set 3rd column
	void setZAxis(const ColumnVec& v)
	{
		setColumn(2, v);
	}

	void setRotationX(const T rad)
	{
		TMat& m = *this;
		T sintheta, costheta;
		sinCos(rad, sintheta, costheta);

		m(0, 0) = T(1);
		m(0, 1) = T(0);
		m(0, 2) = T(0);
		m(1, 0) = T(0);
		m(1, 1) = costheta;
		m(1, 2) = -sintheta;
		m(2, 0) = T(0);
		m(2, 1) = sintheta;
		m(2, 2) = costheta;
	}

	void setRotationY(const T rad)
	{
		TMat& m = *this;
		T sintheta, costheta;
		sinCos(rad, sintheta, costheta);

		m(0, 0) = costheta;
		m(0, 1) = T(0);
		m(0, 2) = sintheta;
		m(1, 0) = T(0);
		m(1, 1) = T(1);
		m(1, 2) = T(0);
		m(2, 0) = -sintheta;
		m(2, 1) = T(0);
		m(2, 2) = costheta;
	}

	void setRotationZ(const T rad)
	{
		TMat& m = *this;
		T sintheta, costheta;
		sinCos(rad, sintheta, costheta);

		m(0, 0) = costheta;
		m(0, 1) = -sintheta;
		m(0, 2) = T(0);
		m(1, 0) = sintheta;
		m(1, 1) = costheta;
		m(1, 2) = T(0);
		m(2, 0) = T(0);
		m(2, 1) = T(0);
		m(2, 2) = T(1);
	}

	/// It rotates "this" in the axis defined by the rotation AND not the world axis.
	void rotateXAxis(const T rad)
	{
		TMat& m = *this;
		// If we analize the mat3 we can extract the 3 unit vectors rotated by the mat3. The 3 rotated vectors are in mat's columns. This means that:
		// mat3.colomn[0] == i * mat3. rotateXAxis() rotates rad angle not from i vector (aka x axis) but from the vector from colomn 0
		// NOTE: See the clean code from < r664

		T sina, cosa;
		sinCos(rad, sina, cosa);

		// zAxis = zAxis*cosa - yAxis*sina;
		m(0, 2) = m(0, 2) * cosa - m(0, 1) * sina;
		m(1, 2) = m(1, 2) * cosa - m(1, 1) * sina;
		m(2, 2) = m(2, 2) * cosa - m(2, 1) * sina;

		// zAxis.normalize();
		T len = sqrt(m(0, 2) * m(0, 2) + m(1, 2) * m(1, 2) + m(2, 2) * m(2, 2));
		m(0, 2) /= len;
		m(1, 2) /= len;
		m(2, 2) /= len;

		// yAxis = zAxis * xAxis;
		m(0, 1) = m(1, 2) * m(2, 0) - m(2, 2) * m(1, 0);
		m(1, 1) = m(2, 2) * m(0, 0) - m(0, 2) * m(2, 0);
		m(2, 1) = m(0, 2) * m(1, 0) - m(1, 2) * m(0, 0);

		// yAxis.normalize();
	}

	/// @copybrief rotateXAxis
	void rotateYAxis(const T rad)
	{
		TMat& m = *this;
		// NOTE: See the clean code from < r664
		T sina, cosa;
		sinCos(rad, sina, cosa);

		// zAxis = zAxis*cosa + xAxis*sina;
		m(0, 2) = m(0, 2) * cosa + m(0, 0) * sina;
		m(1, 2) = m(1, 2) * cosa + m(1, 0) * sina;
		m(2, 2) = m(2, 2) * cosa + m(2, 0) * sina;

		// zAxis.normalize();
		T len = sqrt(m(0, 2) * m(0, 2) + m(1, 2) * m(1, 2) + m(2, 2) * m(2, 2));
		m(0, 2) /= len;
		m(1, 2) /= len;
		m(2, 2) /= len;

		// xAxis = (zAxis*yAxis) * -1.0f;
		m(0, 0) = m(2, 2) * m(1, 1) - m(1, 2) * m(2, 1);
		m(1, 0) = m(0, 2) * m(2, 1) - m(2, 2) * m(0, 1);
		m(2, 0) = m(1, 2) * m(0, 1) - m(0, 2) * m(1, 1);
	}

	/// @copybrief rotateXAxis
	void rotateZAxis(const T rad)
	{
		TMat& m = *this;
		// NOTE: See the clean code from < r664
		T sina, cosa;
		sinCos(rad, sina, cosa);

		// xAxis = xAxis*cosa + yAxis*sina;
		m(0, 0) = m(0, 0) * cosa + m(0, 1) * sina;
		m(1, 0) = m(1, 0) * cosa + m(1, 1) * sina;
		m(2, 0) = m(2, 0) * cosa + m(2, 1) * sina;

		// xAxis.normalize();
		T len = sqrt(m(0, 0) * m(0, 0) + m(1, 0) * m(1, 0) + m(2, 0) * m(2, 0));
		m(0, 0) /= len;
		m(1, 0) /= len;
		m(2, 0) /= len;

		// yAxis = zAxis*xAxis;
		m(0, 1) = m(1, 2) * m(2, 0) - m(2, 2) * m(1, 0);
		m(1, 1) = m(2, 2) * m(0, 0) - m(0, 2) * m(2, 0);
		m(2, 1) = m(0, 2) * m(1, 0) - m(1, 2) * m(0, 0);
	}

	void setRotationPart(const TMat<T, 3, 3>& m3)
	{
		TMat& m = *this;
		for(U j = 0; j < 3; j++)
		{
			for(U i = 0; i < 3; i++)
			{
				m(j, i) = m3(j, i);
			}
		}
	}

	void setRotationPart(const TQuat<T>& q)
	{
		TMat& m = *this;
		// If length is > 1 + 0.002 or < 1 - 0.002 then not normalized quat
		ANKI_ASSERT(absolute(T(1) - q.getLength()) <= 0.002);

		T xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;

		xs = q.x() + q.x();
		ys = q.y() + q.y();
		zs = q.z() + q.z();
		wx = q.w() * xs;
		wy = q.w() * ys;
		wz = q.w() * zs;
		xx = q.x() * xs;
		xy = q.x() * ys;
		xz = q.x() * zs;
		yy = q.y() * ys;
		yz = q.y() * zs;
		zz = q.z() * zs;

		m(0, 0) = T(1) - (yy + zz);
		m(0, 1) = xy - wz;
		m(0, 2) = xz + wy;

		m(1, 0) = xy + wz;
		m(1, 1) = T(1) - (xx + zz);
		m(1, 2) = yz - wx;

		m(2, 0) = xz - wy;
		m(2, 1) = yz + wx;
		m(2, 2) = T(1) - (xx + yy);
	}

	void setRotationPart(const TEuler<T>& e)
	{
		TMat& m = *this;
		T ch, sh, ca, sa, cb, sb;
		sinCos(e.y(), sh, ch);
		sinCos(e.z(), sa, ca);
		sinCos(e.x(), sb, cb);

		m(0, 0) = ch * ca;
		m(0, 1) = sh * sb - ch * sa * cb;
		m(0, 2) = ch * sa * sb + sh * cb;
		m(1, 0) = sa;
		m(1, 1) = ca * cb;
		m(1, 2) = -ca * sb;
		m(2, 0) = -sh * ca;
		m(2, 1) = sh * sa * cb + ch * sb;
		m(2, 2) = -sh * sa * sb + ch * cb;
	}

	void setRotationPart(const TAxisang<T>& axisang)
	{
		TMat& m = *this;
		// Not normalized axis
		ANKI_ASSERT(isZero<T>(T(1) - axisang.getAxis().getLength()));

		T c, s;
		sinCos(axisang.getAngle(), s, c);
		T t = T(1) - c;

		const TVec<T, 3>& axis = axisang.getAxis();
		m(0, 0) = c + axis.x() * axis.x() * t;
		m(1, 1) = c + axis.y() * axis.y() * t;
		m(2, 2) = c + axis.z() * axis.z() * t;

		T tmp1 = axis.x() * axis.y() * t;
		T tmp2 = axis.z() * s;
		m(1, 0) = tmp1 + tmp2;
		m(0, 1) = tmp1 - tmp2;
		tmp1 = axis.x() * axis.z() * t;
		tmp2 = axis.y() * s;
		m(2, 0) = tmp1 - tmp2;
		m(0, 2) = tmp1 + tmp2;
		tmp1 = axis.y() * axis.z() * t;
		tmp2 = axis.x() * s;
		m(2, 1) = tmp1 + tmp2;
		m(1, 2) = tmp1 - tmp2;
	}

	TMat<T, 3, 3> getRotationPart() const
	{
		const TMat& m = *this;
		TMat<T, 3, 3> m3;
		m3(0, 0) = m(0, 0);
		m3(0, 1) = m(0, 1);
		m3(0, 2) = m(0, 2);
		m3(1, 0) = m(1, 0);
		m3(1, 1) = m(1, 1);
		m3(1, 2) = m(1, 2);
		m3(2, 0) = m(2, 0);
		m3(2, 1) = m(2, 1);
		m3(2, 2) = m(2, 2);
		return m3;
	}

	void setTranslationPart(const TVec<T, 3>& v)
	{
		auto c = getColumn(3);
		c.x() = v.x();
		c.y() = v.y();
		c.z() = v.z();
		setColumn(3, c);
	}

	ColumnVec getTranslationPart() const
	{
		return getColumn(3);
	}

	void reorthogonalize()
	{
		// There are 2 methods, the standard and the Gram-Schmidt method with a twist for zAxis. This uses the 2nd. For the first see < r664
		ColumnVec xAxis, yAxis, zAxis;
		getColumns(xAxis, yAxis, zAxis);

		xAxis.normalize();

		yAxis = yAxis - (xAxis * xAxis.dot(yAxis));
		yAxis.normalize();

		zAxis = xAxis.cross(yAxis);

		setColumns(xAxis, yAxis, zAxis);
	}

	void transpose() requires(kIsSquare && !kHasSimd)
	{
		for(U j = 0; j < kTRowCount; j++)
		{
			for(U i = j + 1; i < kTColumnCount; i++)
			{
				T tmp = m_arr2[j][i];
				m_arr2[j][i] = m_arr2[i][j];
				m_arr2[i][j] = tmp;
			}
		}
	}

#if ANKI_ENABLE_SIMD
	void transpose() requires(kIsSquare&& kHasSimd)
	{
#	if ANKI_SIMD_SSE
		_MM_TRANSPOSE4_PS(m_simd[0], m_simd[1], m_simd[2], m_simd[3]);
#	else
		const float32x4x2_t row01 = vtrnq_f32(m_simd[0], m_simd[1]);
		const float32x4x2_t row23 = vtrnq_f32(m_simd[2], m_simd[3]);
		m_simd[0] = vcombine_f32(vget_low_f32(row01.val[0]), vget_low_f32(row23.val[0]));
		m_simd[1] = vcombine_f32(vget_low_f32(row01.val[1]), vget_low_f32(row23.val[1]));
		m_simd[2] = vcombine_f32(vget_high_f32(row01.val[0]), vget_high_f32(row23.val[0]));
		m_simd[3] = vcombine_f32(vget_high_f32(row01.val[1]), vget_high_f32(row23.val[1]));
#	endif
	}
#endif

	void transposeRotationPart()
	{
		for(U j = 0; j < 3; j++)
		{
			for(U i = j + 1; i < 3; i++)
			{
				const T tmp = m_arr2[j][i];
				m_arr2[j][i] = m_arr2[i][j];
				m_arr2[i][j] = tmp;
			}
		}
	}

	TMat getTransposed() const requires(kIsSquare)
	{
		TMat out;
		for(U j = 0; j < kTRowCount; j++)
		{
			for(U i = 0; i < kTColumnCount; i++)
			{
				out.m_arr2[i][j] = m_arr2[j][i];
			}
		}
		return out;
	}

	T getDet() const requires(kSize == 9)
	{
		const auto& m = *this;
		// For the accurate method see < r664
		return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) - m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0))
			   + m(0, 2) * (m(0, 1) * m(2, 1) - m(1, 1) * m(2, 0));
	}

	T getDet() const requires(kSize == 16)
	{
		const auto& t = *this;
		return t(0, 3) * t(1, 2) * t(2, 1) * t(3, 0) - t(0, 2) * t(1, 3) * t(2, 1) * t(3, 0) - t(0, 3) * t(1, 1) * t(2, 2) * t(3, 0)
			   + t(0, 1) * t(1, 3) * t(2, 2) * t(3, 0) + t(0, 2) * t(1, 1) * t(2, 3) * t(3, 0) - t(0, 1) * t(1, 2) * t(2, 3) * t(3, 0)
			   - t(0, 3) * t(1, 2) * t(2, 0) * t(3, 1) + t(0, 2) * t(1, 3) * t(2, 0) * t(3, 1) + t(0, 3) * t(1, 0) * t(2, 2) * t(3, 1)
			   - t(0, 0) * t(1, 3) * t(2, 2) * t(3, 1) - t(0, 2) * t(1, 0) * t(2, 3) * t(3, 1) + t(0, 0) * t(1, 2) * t(2, 3) * t(3, 1)
			   + t(0, 3) * t(1, 1) * t(2, 0) * t(3, 2) - t(0, 1) * t(1, 3) * t(2, 0) * t(3, 2) - t(0, 3) * t(1, 0) * t(2, 1) * t(3, 2)
			   + t(0, 0) * t(1, 3) * t(2, 1) * t(3, 2) + t(0, 1) * t(1, 0) * t(2, 3) * t(3, 2) - t(0, 0) * t(1, 1) * t(2, 3) * t(3, 2)
			   - t(0, 2) * t(1, 1) * t(2, 0) * t(3, 3) + t(0, 1) * t(1, 2) * t(2, 0) * t(3, 3) + t(0, 2) * t(1, 0) * t(2, 1) * t(3, 3)
			   - t(0, 0) * t(1, 2) * t(2, 1) * t(3, 3) - t(0, 1) * t(1, 0) * t(2, 2) * t(3, 3) + t(0, 0) * t(1, 1) * t(2, 2) * t(3, 3);
	}

	TMat getInverse() const requires(kSize == 9)
	{
		// Using Gramer's method Inv(A) = (1 / getDet(A)) * Adj(A)
		const TMat& m = *this;
		TMat r;

		// compute determinant
		const T cofactor0 = m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1);
		const T cofactor3 = m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2);
		const T cofactor6 = m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1);
		const T det = m(0, 0) * cofactor0 + m(1, 0) * cofactor3 + m(2, 0) * cofactor6;

		ANKI_ASSERT(!isZero<T>(det)); // Cannot invert det == 0

		// create adjoint matrix and multiply by 1/det to get inverse
		const T invDet = T(1) / det;
		r(0, 0) = invDet * cofactor0;
		r(0, 1) = invDet * cofactor3;
		r(0, 2) = invDet * cofactor6;

		r(1, 0) = invDet * (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2));
		r(1, 1) = invDet * (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0));
		r(1, 2) = invDet * (m(0, 2) * m(1, 0) - m(0, 0) * m(1, 2));

		r(2, 0) = invDet * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
		r(2, 1) = invDet * (m(0, 1) * m(2, 0) - m(0, 0) * m(2, 1));
		r(2, 2) = invDet * (m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0));

		return r;
	}

	/// Invert using Cramer's rule
	TMat getInverse() const requires(kSize == 16)
	{
		Array<T, 12> tmp;
		const auto& in = (*this);
		TMat m4;

		tmp[0] = in(2, 2) * in(3, 3);
		tmp[1] = in(3, 2) * in(2, 3);
		tmp[2] = in(1, 2) * in(3, 3);
		tmp[3] = in(3, 2) * in(1, 3);
		tmp[4] = in(1, 2) * in(2, 3);
		tmp[5] = in(2, 2) * in(1, 3);
		tmp[6] = in(0, 2) * in(3, 3);
		tmp[7] = in(3, 2) * in(0, 3);
		tmp[8] = in(0, 2) * in(2, 3);
		tmp[9] = in(2, 2) * in(0, 3);
		tmp[10] = in(0, 2) * in(1, 3);
		tmp[11] = in(1, 2) * in(0, 3);

		m4(0, 0) = tmp[0] * in(1, 1) + tmp[3] * in(2, 1) + tmp[4] * in(3, 1);
		m4(0, 0) -= tmp[1] * in(1, 1) + tmp[2] * in(2, 1) + tmp[5] * in(3, 1);
		m4(0, 1) = tmp[1] * in(0, 1) + tmp[6] * in(2, 1) + tmp[9] * in(3, 1);
		m4(0, 1) -= tmp[0] * in(0, 1) + tmp[7] * in(2, 1) + tmp[8] * in(3, 1);
		m4(0, 2) = tmp[2] * in(0, 1) + tmp[7] * in(1, 1) + tmp[10] * in(3, 1);
		m4(0, 2) -= tmp[3] * in(0, 1) + tmp[6] * in(1, 1) + tmp[11] * in(3, 1);
		m4(0, 3) = tmp[5] * in(0, 1) + tmp[8] * in(1, 1) + tmp[11] * in(2, 1);
		m4(0, 3) -= tmp[4] * in(0, 1) + tmp[9] * in(1, 1) + tmp[10] * in(2, 1);
		m4(1, 0) = tmp[1] * in(1, 0) + tmp[2] * in(2, 0) + tmp[5] * in(3, 0);
		m4(1, 0) -= tmp[0] * in(1, 0) + tmp[3] * in(2, 0) + tmp[4] * in(3, 0);
		m4(1, 1) = tmp[0] * in(0, 0) + tmp[7] * in(2, 0) + tmp[8] * in(3, 0);
		m4(1, 1) -= tmp[1] * in(0, 0) + tmp[6] * in(2, 0) + tmp[9] * in(3, 0);
		m4(1, 2) = tmp[3] * in(0, 0) + tmp[6] * in(1, 0) + tmp[11] * in(3, 0);
		m4(1, 2) -= tmp[2] * in(0, 0) + tmp[7] * in(1, 0) + tmp[10] * in(3, 0);
		m4(1, 3) = tmp[4] * in(0, 0) + tmp[9] * in(1, 0) + tmp[10] * in(2, 0);
		m4(1, 3) -= tmp[5] * in(0, 0) + tmp[8] * in(1, 0) + tmp[11] * in(2, 0);

		tmp[0] = in(2, 0) * in(3, 1);
		tmp[1] = in(3, 0) * in(2, 1);
		tmp[2] = in(1, 0) * in(3, 1);
		tmp[3] = in(3, 0) * in(1, 1);
		tmp[4] = in(1, 0) * in(2, 1);
		tmp[5] = in(2, 0) * in(1, 1);
		tmp[6] = in(0, 0) * in(3, 1);
		tmp[7] = in(3, 0) * in(0, 1);
		tmp[8] = in(0, 0) * in(2, 1);
		tmp[9] = in(2, 0) * in(0, 1);
		tmp[10] = in(0, 0) * in(1, 1);
		tmp[11] = in(1, 0) * in(0, 1);

		m4(2, 0) = tmp[0] * in(1, 3) + tmp[3] * in(2, 3) + tmp[4] * in(3, 3);
		m4(2, 0) -= tmp[1] * in(1, 3) + tmp[2] * in(2, 3) + tmp[5] * in(3, 3);
		m4(2, 1) = tmp[1] * in(0, 3) + tmp[6] * in(2, 3) + tmp[9] * in(3, 3);
		m4(2, 1) -= tmp[0] * in(0, 3) + tmp[7] * in(2, 3) + tmp[8] * in(3, 3);
		m4(2, 2) = tmp[2] * in(0, 3) + tmp[7] * in(1, 3) + tmp[10] * in(3, 3);
		m4(2, 2) -= tmp[3] * in(0, 3) + tmp[6] * in(1, 3) + tmp[11] * in(3, 3);
		m4(2, 3) = tmp[5] * in(0, 3) + tmp[8] * in(1, 3) + tmp[11] * in(2, 3);
		m4(2, 3) -= tmp[4] * in(0, 3) + tmp[9] * in(1, 3) + tmp[10] * in(2, 3);
		m4(3, 0) = tmp[2] * in(2, 2) + tmp[5] * in(3, 2) + tmp[1] * in(1, 2);
		m4(3, 0) -= tmp[4] * in(3, 2) + tmp[0] * in(1, 2) + tmp[3] * in(2, 2);
		m4(3, 1) = tmp[8] * in(3, 2) + tmp[0] * in(0, 2) + tmp[7] * in(2, 2);
		m4(3, 1) -= tmp[6] * in(2, 2) + tmp[9] * in(3, 2) + tmp[1] * in(0, 2);
		m4(3, 2) = tmp[6] * in(1, 2) + tmp[11] * in(3, 2) + tmp[3] * in(0, 2);
		m4(3, 2) -= tmp[10] * in(3, 2) + tmp[2] * in(0, 2) + tmp[7] * in(1, 2);
		m4(3, 3) = tmp[10] * in(2, 2) + tmp[4] * in(0, 2) + tmp[9] * in(1, 2);
		m4(3, 3) -= tmp[8] * in(1, 2) + tmp[11] * in(2, 2) + tmp[5] * in(0, 2);

		T det = in(0, 0) * m4(0, 0) + in(1, 0) * m4(0, 1) + in(2, 0) * m4(0, 2) + in(3, 0) * m4(0, 3);

		ANKI_ASSERT(!isZero<T>(det)); // Cannot invert, det == 0
		det = T(1) / det;
		m4 *= det;
		return m4;
	}

	/// See getInverse
	void invert() requires(kSize == 16 || kSize == 9)
	{
		(*this) = getInverse();
	}

	/// 12 muls, 27 adds. Something like m4 = m0 * m1 but without touching the 4rth row and allot faster
	static TMat combineTransformations(const TMat& m0, const TMat& m1) requires(kSize == 16)
	{
		// See the clean code in < r664

		// one of the 2 mat4 doesnt represent transformation
		ANKI_ASSERT(isZero<T>(m0(3, 0) + m0(3, 1) + m0(3, 2) + m0(3, 3) - T(1)) && isZero<T>(m1(3, 0) + m1(3, 1) + m1(3, 2) + m1(3, 3) - T(1)));

		TMat m4;

		m4(0, 0) = m0(0, 0) * m1(0, 0) + m0(0, 1) * m1(1, 0) + m0(0, 2) * m1(2, 0);
		m4(0, 1) = m0(0, 0) * m1(0, 1) + m0(0, 1) * m1(1, 1) + m0(0, 2) * m1(2, 1);
		m4(0, 2) = m0(0, 0) * m1(0, 2) + m0(0, 1) * m1(1, 2) + m0(0, 2) * m1(2, 2);
		m4(1, 0) = m0(1, 0) * m1(0, 0) + m0(1, 1) * m1(1, 0) + m0(1, 2) * m1(2, 0);
		m4(1, 1) = m0(1, 0) * m1(0, 1) + m0(1, 1) * m1(1, 1) + m0(1, 2) * m1(2, 1);
		m4(1, 2) = m0(1, 0) * m1(0, 2) + m0(1, 1) * m1(1, 2) + m0(1, 2) * m1(2, 2);
		m4(2, 0) = m0(2, 0) * m1(0, 0) + m0(2, 1) * m1(1, 0) + m0(2, 2) * m1(2, 0);
		m4(2, 1) = m0(2, 0) * m1(0, 1) + m0(2, 1) * m1(1, 1) + m0(2, 2) * m1(2, 1);
		m4(2, 2) = m0(2, 0) * m1(0, 2) + m0(2, 1) * m1(1, 2) + m0(2, 2) * m1(2, 2);

		m4(0, 3) = m0(0, 0) * m1(0, 3) + m0(0, 1) * m1(1, 3) + m0(0, 2) * m1(2, 3) + m0(0, 3);

		m4(1, 3) = m0(1, 0) * m1(0, 3) + m0(1, 1) * m1(1, 3) + m0(1, 2) * m1(2, 3) + m0(1, 3);

		m4(2, 3) = m0(2, 0) * m1(0, 3) + m0(2, 1) * m1(1, 3) + m0(2, 2) * m1(2, 3) + m0(2, 3);

		m4(3, 0) = m4(3, 1) = m4(3, 2) = T(0);
		m4(3, 3) = T(1);

		return m4;
	}

	/// Create a new matrix that is equivalent to Mat4(this)*Mat4(b)
	[[nodiscard]] TMat combineTransformations(const TMat& b) const requires(kSize == 12 && !kHasSimd)
	{
		const auto& a = *this;
		TMat c;

		c(0, 0) = a(0, 0) * b(0, 0) + a(0, 1) * b(1, 0) + a(0, 2) * b(2, 0);
		c(0, 1) = a(0, 0) * b(0, 1) + a(0, 1) * b(1, 1) + a(0, 2) * b(2, 1);
		c(0, 2) = a(0, 0) * b(0, 2) + a(0, 1) * b(1, 2) + a(0, 2) * b(2, 2);
		c(1, 0) = a(1, 0) * b(0, 0) + a(1, 1) * b(1, 0) + a(1, 2) * b(2, 0);
		c(1, 1) = a(1, 0) * b(0, 1) + a(1, 1) * b(1, 1) + a(1, 2) * b(2, 1);
		c(1, 2) = a(1, 0) * b(0, 2) + a(1, 1) * b(1, 2) + a(1, 2) * b(2, 2);
		c(2, 0) = a(2, 0) * b(0, 0) + a(2, 1) * b(1, 0) + a(2, 2) * b(2, 0);
		c(2, 1) = a(2, 0) * b(0, 1) + a(2, 1) * b(1, 1) + a(2, 2) * b(2, 1);
		c(2, 2) = a(2, 0) * b(0, 2) + a(2, 1) * b(1, 2) + a(2, 2) * b(2, 2);

		c(0, 3) = a(0, 0) * b(0, 3) + a(0, 1) * b(1, 3) + a(0, 2) * b(2, 3) + a(0, 3);

		c(1, 3) = a(1, 0) * b(0, 3) + a(1, 1) * b(1, 3) + a(1, 2) * b(2, 3) + a(1, 3);

		c(2, 3) = a(2, 0) * b(0, 3) + a(2, 1) * b(1, 3) + a(2, 2) * b(2, 3) + a(2, 3);

		return c;
	}

#if ANKI_ENABLE_SIMD
	[[nodiscard]] TMat combineTransformations(const TMat& b) const requires(kSize == 12 && kHasSimd)
	{
		TMat c;
		const auto& a = *this;
#	if ANKI_SIMD_SSE
		for(U i = 0; i < 3; i++)
		{
			__m128 t1, t2;

			t1 = _mm_set1_ps(a(i, 0));
			t2 = _mm_mul_ps(b.m_simd[0], t1);
			t1 = _mm_set1_ps(a(i, 1));
			t2 = _mm_add_ps(_mm_mul_ps(b.m_simd[1], t1), t2);
			t1 = _mm_set1_ps(a(i, 2));
			t2 = _mm_add_ps(_mm_mul_ps(b.m_simd[2], t1), t2);

			TVec<T, 4> v4(T(0), T(0), T(0), a(i, 3));
			t2 = _mm_add_ps(v4.getSimd(), t2);

			c.m_simd[i] = t2;
		}
#	else
		for(U i = 0; i < 3; i++)
		{
			float32x4_t t1, t2;

			t1 = vdupq_n_f32(a(i, 0));
			t2 = b.m_simd[0] * t1;
			t1 = vdupq_n_f32(a(i, 1));
			t2 = b.m_simd[1] * t1 + t2;
			t1 = vdupq_n_f32(a(i, 2));
			t2 = b.m_simd[2] * t1 + t2;

			TVec<T, 4> v4(T(0), T(0), T(0), a(i, 3));
			t2 += v4.getSimd();

			c.m_simd[i] = t2;
		}
#	endif

		return c;
	}
#endif

	/// Calculate a perspective projection matrix. The z is mapped in [0, 1] range just like DX and Vulkan.
	/// Same as D3DXMatrixPerspectiveFovRH
	[[nodiscard]] static TMat calculatePerspectiveProjectionMatrix(T fovX, T fovY, T near, T far) requires(kSize == 16)
	{
		ANKI_ASSERT(fovX > T(0) && fovY > T(0) && near > T(0) && far > T(0));
		const T g = near - far;
		const T f = T(1) / tan(fovY / T(2)); // f = cot(fovY/2)

		TMat proj;
		proj(0, 0) = f * (fovY / fovX); // = f/aspectRatio;
		proj(0, 1) = T(0);
		proj(0, 2) = T(0);
		proj(0, 3) = T(0);
		proj(1, 0) = T(0);
		proj(1, 1) = f;
		proj(1, 2) = T(0);
		proj(1, 3) = T(0);
		proj(2, 0) = T(0);
		proj(2, 1) = T(0);
		proj(2, 2) = far / g;
		proj(2, 3) = (far * near) / g;
		proj(3, 0) = T(0);
		proj(3, 1) = T(0);
		proj(3, 2) = T(-1);
		proj(3, 3) = T(0);

		return proj;
	}

	/// Calculate an orthographic projection matrix. The z is mapped in [0, 1] range just like DX and Vulkan.
	/// Same as D3DXMatrixOrthoOffCenterRH.
	[[nodiscard]] static TMat calculateOrthographicProjectionMatrix(T right, T left, T top, T bottom, T near, T far) requires(kSize == 16)
	{
		ANKI_ASSERT(right != T(0) && left != T(0) && top != T(0) && bottom != T(0) && near != T(0) && far != T(0));
		const T difx = right - left;
		const T dify = top - bottom;
		const T difz = far - near;
		const T tx = -(right + left) / difx;
		const T ty = -(top + bottom) / dify;
		const T tz = -near / difz;
		TMat m;

		m(0, 0) = T(2) / difx;
		m(0, 1) = T(0);
		m(0, 2) = T(0);
		m(0, 3) = tx;
		m(1, 0) = T(0);
		m(1, 1) = T(2) / dify;
		m(1, 2) = T(0);
		m(1, 3) = ty;
		m(2, 0) = T(0);
		m(2, 1) = T(0);
		m(2, 2) = T(-1) / difz;
		m(2, 3) = tz;
		m(3, 0) = T(0);
		m(3, 1) = T(0);
		m(3, 2) = T(0);
		m(3, 3) = T(1);

		return m;
	}

	/// Calculate a perspective projection matrix. The z is reversed and mapped in [1, 0] range just like DX and Vulkan.
	/// Same as D3DXMatrixPerspectiveFovRH but z reversed
	[[nodiscard]] static TMat calculatePerspectiveProjectionMatrixReverseZ(T fovX, T fovY, T near, T far) requires(kSize == 16)
	{
		ANKI_ASSERT(fovX > T(0) && fovY > T(0) && near > T(0) && far > T(0));
		const T g = near - far;
		const T f = T(1) / tan(fovY / T(2)); // f = cot(fovY/2)

		TMat proj;
		proj(0, 0) = f * (fovY / fovX); // = f/aspectRatio;
		proj(0, 1) = T(0);
		proj(0, 2) = T(0);
		proj(0, 3) = T(0);
		proj(1, 0) = T(0);
		proj(1, 1) = f;
		proj(1, 2) = T(0);
		proj(1, 3) = T(0);
		proj(2, 0) = T(0);
		proj(2, 1) = T(0);
		proj(2, 2) = far / g;
		proj(2, 3) = (far * near) / g;
		proj(3, 0) = T(0);
		proj(3, 1) = T(0);
		proj(3, 2) = T(-1);
		proj(3, 3) = T(0);

		const TMat rev(T(1), T(0), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(-1), T(1), T(0), T(0), T(0), T(1));

		return rev * proj;
	}

	/// Given the parameters that construct a projection matrix extract 4 values that can be used to unproject a point from NDC to view space.
	/// @code
	/// Vec4 unprojParams = calculatePerspectiveUnprojectionParams(...);
	/// F32 z = unprojParams.z() / (unprojParams.w() + depth);
	/// Vec2 xy = ndc.xy() * unprojParams.xy() * z;
	/// Vec3 posViewSpace(xy, z);
	/// @endcode
	static TVec<T, 4> calculatePerspectiveUnprojectionParams(T fovX, T fovY, T near, T far) requires(kSize == 16)
	{
		TVec<T, 4> out;
		const T g = near - far;
		const T f = T(1) / tan(fovY / T(2)); // f = cot(fovY/2)

		const T m00 = f * (fovY / fovX);
		const T m11 = f;
		const T m22 = far / g;
		const T m23 = (far * near) / g;

		// First, clip = (m * Pv) where Pv is the view space position.
		// ndc.z = clip.z / clip.w = (m22 * Pv.z + m23) / -Pv.z. Note that ndc.z == depth in zero_to_one projection.
		// Solving that for Pv.z we get
		// Pv.z = A / (depth + B)
		// where A = -m23 and B = m22
		// so we save the A and B in the projection params vector
		out.z() = -m23;
		out.w() = m22;

		// Using the same logic the Pv.x = x' * w / m00
		// so Pv.x = x' * Pv.z * (-1 / m00)
		out.x() = -T(T(1)) / m00;

		// Same for y
		out.y() = -T(T(1)) / m11;

		return out;
	}

	/// Assuming this is a projection matrix extract the unprojection parameters. See calculatePerspectiveUnprojectionParams for more info.
	TVec<T, 4> extractPerspectiveUnprojectionParams() const requires(kSize == 16)
	{
		TVec<T, 4> out;
		const auto& m = *this;
		out.z() = -m(2, 3);
		out.w() = m(2, 2);
		out.x() = -T(T(1)) / m(0, 0);
		out.y() = -T(T(1)) / m(1, 1);
		return out;
	}

	/// If we suppose this matrix represents a transformation, return the inverted transformation
	TMat getInverseTransformation() const requires(kSize == 16)
	{
		const TMat<T, 3, 3> invertedRot = getRotationPart().getTransposed();
		TVec<T, 3> invertedTsl = getTranslationPart().xyz();
		invertedTsl = -(invertedRot * invertedTsl);
		return TMat(invertedTsl.xyz0(), invertedRot);
	}

	/// If we suppose this matrix represents a transformation, return the inverted transformation
	TMat getInverseTransformation() const requires(kSize == 12)
	{
		const TMat<T, 3, 3> invertedRot = getRotationPart().getTransposed();
		TVec<T, 3> invertedTsl = getTranslationPart().xyz();
		invertedTsl = -(invertedRot * invertedTsl);
		return TMat(invertedTsl.xyz(), invertedRot);
	}

	/// @note 9 muls, 9 adds
	TVec<T, 3> transform(const TVec<T, 3>& v) const requires(kSize == 16)
	{
		const auto& m = *this;
		return TVec<T, 3>(m(0, 0) * v.x() + m(0, 1) * v.y() + m(0, 2) * v.z() + m(0, 3),
						  m(1, 0) * v.x() + m(1, 1) * v.y() + m(1, 2) * v.z() + m(1, 3),
						  m(2, 0) * v.x() + m(2, 1) * v.y() + m(2, 2) * v.z() + m(2, 3));
	}

	/// Create a new transform matrix position at eye and looking at refPoint.
	template<U kVecDimensions>
	static TMat lookAt(const TVec<T, kVecDimensions>& eye, const TVec<T, kVecDimensions>& refPoint,
					   const TVec<T, kVecDimensions>& up) requires(kTRowCount == 3 && kTColumnCount == 4 && kVecDimensions >= 3)
	{
		const TVec<T, 3> vdir = (refPoint.xyz() - eye.xyz()).getNormalized();
		const TVec<T, 3> vup = (up.xyz() - vdir * up.xyz().dot(vdir)).getNormalized();
		const TVec<T, 3> vside = vdir.cross(vup);
		TMat out;
		out.setColumns(vside, vup, -vdir, eye.xyz());
		return out;
	}

	/// Create a new transform matrix position at eye and looking at refPoint.
	template<U kVecDimensions>
	static TMat lookAt(const TVec<T, kVecDimensions>& eye, const TVec<T, kVecDimensions>& refPoint,
					   const TVec<T, kVecDimensions>& up) requires(kTRowCount == 4 && kTColumnCount == 4 && kVecDimensions >= 3)
	{
		const TVec<T, 4> vdir = (refPoint.xyz0() - eye.xyz0()).getNormalized();
		const TVec<T, 4> vup = (up.xyz0() - vdir * up.xyz0().dot(vdir)).getNormalized();
		const TVec<T, 4> vside = vdir.cross(vup);
		TMat out;
		out.setColumns(vside, vup, -vdir, eye.xyz1());
		return out;
	}

	/// Create a rotation matrix from some direction. http://jcgt.org/published/0006/01/01/
	static TMat rotationFromDirection(const TVec<T, 3>& zAxis) requires(kSize == 9)
	{
		const TVec<T, 3> z = zAxis;
		const T sign = (z.z() >= T(0)) ? T(1) : -T(1);
		const T a = -T(1) / (sign + z.z());
		const T b = z.x() * z.y() * a;

		const TVec<T, 3> x = TVec<T, 3>(T(1) + sign * a * pow(z.x(), T(2)), sign * b, -sign * z.x());
		const TVec<T, 3> y = TVec<T, 3>(b, sign + a * pow(z.y(), T(2)), -z.y());

		TMat out;
		out.setColumns(x, y, z);
		return out;
	}

	TMat lerp(const TMat& b, T t) const
	{
		return ((*this) * (T(1) - t)) + (b * t);
	}

	static TMat getZero()
	{
		return TMat(T(0));
	}

	void setZero()
	{
		*this = getZero();
	}

	static TMat getIdentity() requires(kSize == 9)
	{
		return TMat(T(1), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(1));
	}

	static TMat getIdentity() requires(kSize == 16)
	{
		return TMat(T(1), T(0), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(1));
	}

	static TMat getIdentity() requires(kSize == 12)
	{
		return TMat(T(1), T(0), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(1), T(0));
	}

	void setIdentity()
	{
		(*this) = getIdentity();
	}

	static constexpr U8 getSize()
	{
		return U8(kTColumnCount * kTRowCount);
	}

	String toString() const requires(std::is_floating_point<T>::value)
	{
		String str;
		for(U j = 0; j < kTRowCount; ++j)
		{
			for(U i = 0; i < kTColumnCount; ++i)
			{
				CString fmt;
				if(i == kTColumnCount - 1 && j == kTRowCount - 1)
				{
					fmt = "%f";
				}
				else if(i == kTColumnCount - 1)
				{
					fmt = "%f\n";
				}
				else
				{
					fmt = "%f ";
				}
				str += String().sprintf(fmt.cstr(), m_arr2[j][i]);
			}
		}
		return str;
	}
	/// @}

protected:
	static constexpr U N = kTColumnCount * kTRowCount;

	/// @name Data members
	/// @{
	union
	{
		T m_carr1[N]; ///< For easier debugging with gdb
		T m_carr2[kTRowCount][kTColumnCount]; ///< For easier debugging with gdb
		Array<T, N> m_arr1;
		Array2d<T, kTRowCount, kTColumnCount> m_arr2;
		SimdArray m_simd;
		Array<RowVec, kTRowCount> m_rows;
	};
	/// @}
};

/// @memberof TMat
template<typename T, U kTRowCount, U kTColumnCount>
TMat<T, kTRowCount, kTColumnCount> operator+(const T f, const TMat<T, kTRowCount, kTColumnCount>& m)
{
	return m + f;
}

/// @memberof TMat
template<typename T, U kTRowCount, U kTColumnCount>
TMat<T, kTRowCount, kTColumnCount> operator-(const T f, const TMat<T, kTRowCount, kTColumnCount>& m)
{
	TMat<T, kTRowCount, kTColumnCount> out;
	for(U i = 0; i < kTRowCount * kTColumnCount; i++)
	{
		out[i] = f - m[i];
	}
	return out;
}

/// @memberof TMat
template<typename T, U kTRowCount, U kTColumnCount>
TMat<T, kTRowCount, kTColumnCount> operator*(const T f, const TMat<T, kTRowCount, kTColumnCount>& m)
{
	return m * f;
}

/// @memberof TMat
template<typename T, U kTRowCount, U kTColumnCount>
TMat<T, kTRowCount, kTColumnCount> operator/(const T f, const TMat<T, 3, 3>& m3)
{
	TMat<T, kTRowCount, kTColumnCount> out;
	for(U i = 0; i < kTRowCount * kTColumnCount; i++)
	{
		ANKI_ASSERT(m3[i] != T(0));
		out[i] = f / m3[i];
	}
	return out;
}

/// F32 3x3 matrix
using Mat3 = TMat<F32, 3, 3>;
static_assert(sizeof(Mat3) == sizeof(F32) * 3 * 3, "Incorrect size");

/// F64 3x3 matrix
using DMat3 = TMat<F64, 3, 3>;
static_assert(sizeof(DMat3) == sizeof(F64) * 3 * 3, "Incorrect size");

/// F32 4x4 matrix
using Mat4 = TMat<F32, 4, 4>;
static_assert(sizeof(Mat4) == sizeof(F32) * 4 * 4, "Incorrect size");

/// F64 4x4 matrix
using DMat4 = TMat<F64, 4, 4>;
static_assert(sizeof(DMat4) == sizeof(F64) * 4 * 4, "Incorrect size");

/// F32 3x4 matrix
using Mat3x4 = TMat<F32, 3, 4>;
static_assert(sizeof(Mat3x4) == sizeof(F32) * 3 * 4, "Incorrect size");

/// F64 3x4 matrix
using DMat3x4 = TMat<F64, 3, 4>;
static_assert(sizeof(DMat3x4) == sizeof(F64) * 3 * 4, "Incorrect size");
/// @}

} // end namespace anki
