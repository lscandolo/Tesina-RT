#pragma once
#ifndef RT_VECTOR_HPP
#define RT_VECTOR_HPP

#include <ostream>
#include <stdint.h>
#include <math.h>

#include <rt/assert.hpp>


template <int N, typename F>
class vec
{
public:

        float v[N];

	inline vec<N,F> () {}

	// inline vec3 (const float& _x, 
	// 	     const float& _y, 
	// 	     const float& _z ) : x(_x), y(_y), z(_z) {}
	// inline vec<N,F> (const vec<N,F>& _v) : v(_v.v){}

	inline vec<N,F> (const float& val) {
		for (uint32_t i = 0; i < N; ++i)
			v[i] = val;
	}
	inline vec<N,F> (const float* vals) {
		for (uint32_t i = 0; i < N; ++i)
			v[i] = vals[i];
	}

/* Operators */
	// inline const vec<N,F>& operator=(const vec<N,F>& rhs ) 
	// 	{ x = rhs.x; y = rhs.y; z = rhs.z; return *this; }

	inline bool operator==(const vec<N,F>& rhs ) const 
		{
			bool ret = true;
			for (uint32_t i = 0; i < N; ++i)
				ret = ret && (v[i] == rhs[i]);
			return ret;
		}
					
	inline bool operator!=(const vec<N,F>& rhs ) const 
		{return !((*this)==rhs);}

	inline const vec<N,F> operator-() const 
		{
			vec<N,F> r;
			for (uint32_t i = 0; i < N; ++i)
				r.v[i] = this->v[i];
			return r;
		}
					
	inline const vec<N,F> operator+( const vec<N,F>& rhs ) const 
		{
			vec<N,F> r;
			for (uint32_t i = 0; i < N; ++i)
				r.v[i] = this->v[i] + rhs.v[i];
			return r;
		}

	inline const vec<N,F> operator-( const vec<N,F>& rhs ) const 
		{ 
			vec<N,F> r;
			for (uint32_t i = 0; i < N; ++i)
				r.v[i] = this->v[i] - rhs.v[i];
			return r;
		}

	inline const vec<N,F> operator*( const float& rhs ) const 
		{
			vec<N,F> r;
			for (uint32_t i = 0; i < N; ++i)
				r.v[i] = this->v[i] * rhs;
			return r;
		}

	inline float operator*( const vec<N,F>& rhs ) const 
		{
			float r = 0;
			for (uint32_t i = 0; i < N; ++i)
				r += this->v[i] * rhs.v[i];
			return r;
		}

	inline const vec<N,F> operator/( const float& rhs ) const
		{
			ASSERT(v != 0.f);
			float rhs_inv = 1.f / rhs;
			vec<N,F> r;
			for (uint32_t i = 0; i < N; ++i)
				r.v[i] = this->v[i] * rhs_inv;
			return r;
		}

	// const vec<N,F> operator/( const vec<N,F>& rhs ) const 
	// 	{
	// 		ASSERT(rhs.x != 0.f && rhs.y != 0.f &&rhs.z != 0.f);
	// 		return vec<N,F>( x/rhs.x, y/rhs.y, z/rhs.z ); 
	// 	};

	inline vec<N,F>& operator+= ( const vec<N,F>& rhs ) 
		{
			for (uint32_t i = 0; i < N; ++i)
				this->v[i] += rhs.v[i];
			return *this;
		}
    
	inline vec<N,F>& operator-= ( const vec<N,F>& rhs ) 
		{
			for (uint32_t i = 0; i < N; ++i)
				this->v[i] -= rhs.v[i];
			return *this;
		}

	inline vec<N,F>& operator*= ( const float& rhs ) 
		{
			for (uint32_t i = 0; i < N; ++i)
				this->v[i] *= rhs;
			return *this;
		}

	inline vec<N,F>& operator/= ( const float& rhs ) 
		{
			ASSERT(rhs != 0.f);
			float rhs_inv = 1.f / rhs;
			for (uint32_t i = 0; i < N; ++i)
				this->v[i] *= rhs_inv;
			return *this;
		}

	// inline vec<N,F>& operator/= ( const vec<N,F>& v ) 
	// 	{x /= v.x; y /= v.y; z /= v.z; return *this; };

	inline float& operator[] (const uint8_t i) 
		{
			ASSERT(i >= 0 && i < N);
			return this->v[i];
		}

	inline float operator[] (const uint8_t i) const
		{
			ASSERT(i >= 0 && i < N);
			return this->v[i];
		}

	float norm()
		{
			float n = 0.f;
			for (uint32_t i = 0; i < N; ++i){
				n += v[i] * v[i];
			}
			return sqrtf(n);
		}

	float sqnorm()
		{
			float n = 0;
			for (uint32_t i = 0; i < N; ++i){
				n += this->v[i] * this->v[i];
			}
			return n;
		}
	
	vec<N,F>& normalize()
		{
			float n = norm();
			ASSERT(n != 0.f);
			(*this)  /= n;
			return *this;
		};

	vec<N,F> normalized() const
		{
			vec<N,F> n;
			for (uint32_t i = 0; i < N; ++i)
				n[i] = v[i];
			n.normalize();
			return n;
		};

        /* Friendly neighborhood stream helper function */
        friend std::ostream& operator<< (std::ostream &out, const vec<N,F>& v) {
                for (uint32_t i = 0; i < N; ++i) {
                        out << v[i] << "\t";
                }
                return out;
        }

	// inline float norm() const
	// 	{return x*x + y*y + z*z;}

	// inline void normalize()
	// 	{ 
	// 		ASSERT(x != 0 || y != 0 || z != 0);
	// 		float inv_norm = 1.f/norm();
	// 		x *= inv_norm;
	// 		y *= inv_norm;
	// 		z *= inv_norm;
	// 	}

	// /* Constructors */
        // inline vec<N,F> ();
        // inline vec<N,F> (const float& _x, 
	// 	     const float& _y, 
	// 	     const float& _z );
        // inline vec<N,F> (const vec<N,F>& v);
        // inline vec<N,F> (const float& val);
        // inline vec<N,F> (const float* vals);

        // /* Operators */
        // inline const vec<N,F>& operator=(const vec<N,F>& rhs );
	// inline bool operator==(const vec<N,F>& rhs ) const;
        // inline bool operator!=(const vec<N,F>& rhs ) const; 
        // inline const vec<N,F> operator-() const;
        // inline const vec<N,F> operator+( const vec<N,F>& rhs ) const;
        // inline const vec<N,F> operator-( const vec<N,F>& rhs ) const;
        // inline const vec<N,F> operator*( const float& v ) const;
        // inline const float operator*( const vec<N,F>& v ) const;
        // inline const vec<N,F> operator/( const float& v ) const;
        // const vec<N,F> operator/( const vec<N,F>& rhs ) const;
        // inline vec<N,F>& operator+= ( const vec<N,F>& rhs );
        // inline vec<N,F>& operator-= ( const vec<N,F>& rhs );
        // inline vec<N,F>& operator*= ( const float& v );
        // inline vec<N,F>& operator/= ( const float& v );
        // inline vec<N,F>& operator/= ( const vec<N,F>& v );
};

typedef vec<2,float> vec2;
typedef vec<3,float> vec3;
typedef vec<4,float> vec4;

/*------------ Special helper funcs ------------------*/
vec2 makeVector(const float& v0, const float& v1);
vec3 makeVector(const float& v0, const float& v1, const float& v2);
vec4 makeVector(const float& v0, const float& v1, const float& v2, const float& v3);

const vec3 vec3_zero = makeVector(0.f,0.f,0.f);

#endif /* RT_VECTOR_HPP */
