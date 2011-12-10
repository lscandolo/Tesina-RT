#include <math.h>

#include <algorithm>
#include <rt/math.hpp>



float lerp( const float& a, const float& b, const float& t )
{
        return a + (b-a)*t;
}

float clamp(float& x, float& xMin, float& xMax )
{
        ASSERT(xMax >= xMin );
        return x <= xMin ? xMin : (x >= xMax ? xMax : x);
}

vec3 inv(const vec3& v)
{
	return makeVector(v[0] == 0.f ? 0.f : 1.0f / v[0],
			  v[1] == 0.f ? 0.f : 1.0f / v[1],
			  v[2] == 0.f ? 0.f : 1.0f / v[2]);
}


/* Reflection and refraction */

vec3 cross(const vec3& v1, const vec3& v2 )
{
        return makeVector( (v1[1]*v2[2]) - (v1[2]*v2[1]),
			   (v1[2]*v2[0]) - (v1[0]*v2[2]),
			   (v1[0]*v2[1]) - (v1[1]*v2[0]) );
};

vec3 reflect(const vec3& N,  /* Surface normal */
		    const vec3& I  )/* Incident vector */
{
	return I - N * (N*I*2);
}

bool Refract(const vec3& I, /*Incident vector*/
		    const vec3& N, /*Surface normal*/
		    float eta,     /*Index of refraction relation (outside/inside) */
		    vec3& R)       /*Refraction direction (if there is any)*/
{
	float IN = I * N;

        float B = (eta*eta)*(1.0f - IN*IN);
        if( B > 1.0 )
        {
		// total internal reflection
		return false;
        }
        else
        {
		float f = (-IN*eta - sqrtf(1.0f-B));
		R = N * f + I * eta;
		return true;
        }
}



