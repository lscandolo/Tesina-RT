#ifndef RT_VEC3_HPP
#define RT_VEC3_HPP

#include <rt/assert.hpp>

class vec3
{
public:

        float x,y,z;


	inline vec3 () {}
	inline vec3 (const float& _x, 
			   const float& _y, 
			   const float& _z ) : x(_x), y(_y), z(_z) {}

	inline vec3 (const vec3& v) : x(v.x), y(v.y), z(v.z) {}
	inline vec3 (const float& val) : x(val), y(val), z(val) {}
	inline vec3 (const float* vals) : x(vals[0]), y(vals[1]), z(vals[2]) {}

/* Operators */
	inline const vec3& operator=(const vec3& rhs ) 
		{ x = rhs.x; y = rhs.y; z = rhs.z; return *this; }

	inline bool operator==(const vec3& rhs ) const 
		{ return ( x == rhs.x && y == rhs.y && z == rhs.z); }
        
	inline bool operator!=(const vec3& rhs ) const 
		{ return ( x != rhs.x || y != rhs.y || z != rhs.z ); }

	inline const vec3 operator-() const 
		{return vec3( -x, -y, -z ); }

	inline const vec3 operator+( const vec3& rhs ) const 
		{ return vec3( x + rhs.x, y + rhs.y, z + rhs.z ); }

	inline const vec3 operator-( const vec3& rhs ) const 
		{ return vec3( x - rhs.x, y - rhs.y, z - rhs.z );}

	inline const vec3 operator*( const float& v ) const 
		{return vec3( x*v, y*v, z*v ); }

	inline const float operator*( const vec3& v ) const 
		{return ( v.x*x + v.y*y + v.z*z ); }

	inline const vec3 operator/( const float& v ) const
		{
			ASSERT(v != 0.f);
			float v_inv = 1.f / v;
			return vec3( x*v_inv, y*v_inv, z*v_inv ); 
		}

	const vec3 operator/( const vec3& rhs ) const 
		{
			ASSERT(rhs.x != 0.f && rhs.y != 0.f &&rhs.z != 0.f);
			return vec3( x/rhs.x, y/rhs.y, z/rhs.z ); 
		};

	inline vec3& operator+= ( const vec3& rhs ) 
		{x += rhs.x; y += rhs.y; z += rhs.z; return *this; };
    
	inline vec3& operator-= ( const vec3& rhs ) 
		{x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; };

	inline vec3& operator*= ( const float& v ) 
		{x *= v; y *= v; z *= v; return *this; };

	inline vec3& operator/= ( const float& v ) 
		{x /= v; y /= v; z /= v; return *this; };

	inline vec3& operator/= ( const vec3& v ) 
		{x /= v.x; y /= v.y; z /= v.z; return *this; };

	inline float norm() const
		{return x*x + y*y + z*z;}

	inline void normalize()
		{ 
			ASSERT(x != 0 || y != 0 || z != 0);
			float inv_norm = 1.f/norm();
			x *= inv_norm;
			y *= inv_norm;
			z *= inv_norm;
		}

	// /* Constructors */
        // inline vec3 ();
        // inline vec3 (const float& _x, 
	// 	     const float& _y, 
	// 	     const float& _z );
        // inline vec3 (const vec3& v);
        // inline vec3 (const float& val);
        // inline vec3 (const float* vals);

        // /* Operators */
        // inline const vec3& operator=(const vec3& rhs );
	// inline bool operator==(const vec3& rhs ) const;
        // inline bool operator!=(const vec3& rhs ) const; 
        // inline const vec3 operator-() const;
        // inline const vec3 operator+( const vec3& rhs ) const;
        // inline const vec3 operator-( const vec3& rhs ) const;
        // inline const vec3 operator*( const float& v ) const;
        // inline const float operator*( const vec3& v ) const;
        // inline const vec3 operator/( const float& v ) const;
        // const vec3 operator/( const vec3& rhs ) const;
        // inline vec3& operator+= ( const vec3& rhs );
        // inline vec3& operator-= ( const vec3& rhs );
        // inline vec3& operator*= ( const float& v );
        // inline vec3& operator/= ( const float& v );
        // inline vec3& operator/= ( const vec3& v );
};

#endif /* RT_VEC3_HPP */
