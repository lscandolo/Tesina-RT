#ifndef RT_MATH_HPP
#define RT_MATH_HPP

#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>

#include <rt/vector.hpp>
#include <rt/matrix.hpp>
#include <rt/assert.hpp>


template <int N, typename F>
const vec<N,F> lerp( const vec<N,F>& a, const vec<N,F>& b, const float& t )
{
	return a + (b-a)*t;
}

template <int N, typename F>
vec<N,F> min(const vec<N,F>& a, const vec<N,F>& b )
{
	vec<N,F> m;
	for (uint32_t i = 0 ; i < N ; ++i){
		m[i] = std::min(a[i], b[i]);
	}
	return m;
}

template <int N, typename F>
vec<N,F> max(const vec<N,F>& a, const vec<N,F>& b )
{
	vec<N,F> m;
	for (uint32_t i = 0 ; i < N ; ++i){
		m[i] = std::max(a[i], b[i]);
	}
	return m;
}

template <int N, typename F>
vec<N,F> inv(const vec<N,F>& v)
{
	vec<N,F> in;
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

vec3 homogenize(const vec4& v);

/* Reflection and refraction */

vec3 reflect(const vec3& n,  /* Surface normal */
	       const vec3& i); /* Incident vector */
bool Refract(const vec3& i, /*Incident vector*/
	     const vec3& n, /*Surface normal*/
	     float eta, /*Index of refraction relation (outside/inside)*/
	     vec3& R);  /*Refraction direction (if there is any)*/

/* Matrix transform creator functions */

mat4x4 translationMatrix4x4(vec3 pos);
mat4x4 rotationMatrix4x4(vec3 rpy);
mat4x4 scaleMatrix4x4(float scale);
mat4x4 scaleMatrix4x4(const vec3& scale);


#endif /* RT_MATH_HPP */
