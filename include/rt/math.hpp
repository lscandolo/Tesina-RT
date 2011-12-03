#ifndef RT_MATH_HPP
#define RT_MATH_HPP

#include <math.h>
#include <rt/vector.hpp>
#include <rt/assert.hpp>


float norm(const vec3* v);
float norm(const vec3& v);
float sqnorm(const vec3* v);
float sqnorm(const vec3& v);
vec3 normalize( const vec3& v);
vec3 cross(const vec3& v1, const vec3& v2 );
float lerp( const float& a, const float& b, const float& t );
vec3 lerp( const vec3& a, const vec3& b, const float& t );
float clamp(float& x, float& xMin, float& xMax );
vec3 min(const vec3& a, const vec3& b );
vec3 max(const vec3& a, const vec3& b );
vec3 inv(const vec3& v);

/* Reflection and refraction */

vec3 reflect(const vec3& N,  /* Surface normal */
		    const vec3& I); /* Incident vector */
bool Refract(const vec3& I, /*Incident vector*/
		    const vec3& N, /*Surface normal*/
		    float eta, /*Index of refraction relation (outside/inside)*/
		    vec3& R);  /*Refraction direction (if there is any)*/

#endif /* RT_MATH_HPP */
