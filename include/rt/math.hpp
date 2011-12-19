#ifndef RT_MATH_HPP
#define RT_MATH_HPP

#include <math.h>
#include <algorithm>

#include <rt/vector.hpp>
#include <rt/assert.hpp>


// float norm(const vec<3>* v);
// float norm(const vec<3>& v);
// float sqnorm(const vec<3>* v);
// float sqnorm(const vec<3>& v);


template <int N>
const vec<N> lerp( const vec<N>& a, const vec<N>& b, const float& t )
{
	return a + (b-a)*t;
}

template <int N>
vec<N> min(const vec<N>& a, const vec<N>& b )
{
	vec<N> m;
	for (uint32_t i = 0 ; i < N ; ++i){
		m[i] = std::min(a[i], b[i]);
	}
	return m;
}

template <int N>
vec<N> max(const vec<N>& a, const vec<N>& b )
{
	vec<N> m;
	for (uint32_t i = 0 ; i < N ; ++i){
		m[i] = std::max(a[i], b[i]);
	}
	return m;
}

template <int N>
vec<N> inv(const vec<N>& v)
{
	vec<N> in;
	for (uint32_t i = 0 ; i < N ; ++i){
		in[i] = (v.v[i] == 0.f ? 0.f : 1.0f / v.v[i]);
	}	
	return in;
}

/* Auxiliary functions */

/* Take angle (in radians) to [-Pi,Pi]*/
float normalize_angle(float a);
float clamp(float& x, float& xMin, float& xMax );
float lerp( const float& a, const float& b, const float& t );
vec3 cross(const vec3& v1, const vec3& v2 );

/* Reflection and refraction */

vec3 reflect(const vec3& n,  /* Surface normal */
	       const vec3& i); /* Incident vector */
bool Refract(const vec3& i, /*Incident vector*/
	     const vec3& n, /*Surface normal*/
	     float eta, /*Index of refraction relation (outside/inside)*/
	     vec3& R);  /*Refraction direction (if there is any)*/

#endif /* RT_MATH_HPP */
