#include <math.h>
#include <algorithm>
#include <rt/math.hpp>


float norm(const vec3* v)
{
	return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
};

float norm(const vec3& v)
{
	return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
};

float sqnorm(const vec3* v)
{
	return v->x * v->x + v->y * v->y + v->z * v->z;
};

float sqnorm(const vec3& v)
{
	return v.x * v.x + v.y * v.y + v.z * v.z;
};


vec3 normalize( const vec3& v)
{
	return v / norm(v);
};

vec3 cross(const vec3& v1, const vec3& v2 )
{
        return vec3( (v1.y*v2.z) - (v1.z*v2.y),
		     (v1.z*v2.x) - (v1.x*v2.z),
		     (v1.x*v2.y) - (v1.y*v2.x) );
};


float lerp( const float& a, const float& b, const float& t )
{
        return a + (b-a)*t;
}

vec3 lerp( const vec3& a, const vec3& b, const float& t )
{
        return a + (b-a)*t;
}


float clamp(float& x, float& xMin, float& xMax )
{
        ASSERT(xMax >= xMin );
        return x <= xMin ? xMin : (x >= xMax ? xMax : x);
}

vec3 min(const vec3& a, const vec3& b )
{
        return vec3(std::min(a.x,b.x), std::min(a.y,b.y), std::min(a.z,b.z) );
}

vec3 max(const vec3& a, const vec3& b )
{
	return vec3(std::max(a.x,b.x), std::max(a.y,b.y), std::max(a.z,b.z) );
}

vec3 inv(const vec3& v)
{
	return vec3(v.x == 0.f ? 0.f : 1.0f / v.x,
		    v.y == 0.f ? 0.f : 1.0f / v.y,
		    v.z == 0.f ? 0.f : 1.0f / v.z);
}



/* Reflection and refraction */

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



