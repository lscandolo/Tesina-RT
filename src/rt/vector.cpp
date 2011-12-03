#include <rt/vector.hpp>


// /* Constructors */
// inline vec3::vec3 () {}
// inline vec3::vec3 (const float& _x, 
// 		   const float& _y, 
// 		   const float& _z ) : x(_x), y(_y), z(_z) {}

// inline vec3::vec3 (const vec3& v) : x(v.x), y(v.y), z(v.z) {}
// inline vec3::vec3 (const float& val) : x(val), y(val), z(val) {}
// inline vec3::vec3 (const float* vals) : x(vals[0]), y(vals[1]), z(vals[2]) {}

// /* Operators */
// inline const vec3& vec3::operator=(const vec3& rhs ) 
// { x = rhs.x; y = rhs.y; z = rhs.z; return *this; }

// inline bool vec3::operator==(const vec3& rhs ) const 
// { return ( x == rhs.x && y == rhs.y && z == rhs.z); }
        
// inline bool vec3::operator!=(const vec3& rhs ) const 
// { return ( x != rhs.x || y != rhs.y || z != rhs.z ); }

// inline const vec3 vec3::operator-() const 
// {return vec3( -x, -y, -z ); }

// inline const vec3 vec3::operator+( const vec3& rhs ) const 
// { return vec3( x + rhs.x, y + rhs.y, z + rhs.z ); }

// inline const vec3 vec3::operator-( const vec3& rhs ) const 
// { return vec3( x - rhs.x, y - rhs.y, z - rhs.z );}

// inline const vec3 vec3::operator*( const float& v ) const 
// {return vec3( x*v, y*v, z*v ); }

// inline const float vec3::operator*( const vec3& v ) const 
// {return ( v.x*x + v.y*y + v.z*z ); }

// inline const vec3 vec3::operator/( const float& v ) const
// {
// 	ASSERT(v != 0.f);
// 	float v_inv = 1.f / v;
// 	return vec3( x*v_inv, y*v_inv, z*v_inv ); 
// }

// const vec3 vec3::operator/( const vec3& rhs ) const 
// {
// 	ASSERT(rhs.x != 0.f && rhs.y != 0.f &&rhs.z != 0.f);
// 	return vec3( x/rhs.x, y/rhs.y, z/rhs.z ); 
// };

// inline vec3& vec3::operator+= ( const vec3& rhs ) 
// {x += rhs.x; y += rhs.y; z += rhs.z; return *this; };
    
// inline vec3& vec3::operator-= ( const vec3& rhs ) 
// {x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; };

// inline vec3& vec3::operator*= ( const float& v ) 
// {x *= v; y *= v; z *= v; return *this; };

// inline vec3& vec3::operator/= ( const float& v ) 
// {x /= v; y /= v; z /= v; return *this; };

// inline vec3& vec3::operator/= ( const vec3& v ) 
// {x /= v.x; y /= v.y; z /= v.z; return *this; };


