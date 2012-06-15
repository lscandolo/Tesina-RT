#include <math.h>

#include <algorithm>
#include <rt/math.hpp>


#define F_M_PI (float)M_PI

float normalize_angle(float a)
{
	float mod_a = fmodf(a,2*F_M_PI) + 2 * F_M_PI;
	mod_a = fmodf(mod_a,2*F_M_PI);
	return mod_a > F_M_PI? mod_a - 2*F_M_PI : mod_a;
}

float lerp( const float& a, const float& b, const float& t )
{
        return a + (b-a)*t;
}

float clamp(float x, float xMin, float xMax )
{
        // ASSERT(xMax >= xMin );
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

vec3 homogenize(const vec4& v)
{
	ASSERT(v[3] != 0);
	vec3 h;
	float invH = 1.f/v[3];
	h[0] = v[0] * invH;
	h[1] = v[1] * invH;
	h[2] = v[2] * invH;
	return h;
}


mat4x4 translationMatrix4x4(vec3 pos)
{
	mat4x4 M(0.f);
	for (uint32_t i = 0; i < 4; ++i)
		M.val(i,i) = 1.f;
	for (uint32_t i = 0; i < 3; ++i)
		M.val(i,3) = pos[i];
	return M;
}

mat4x4 rotationMatrix4x4(vec3 rpy)
{
	mat4x4 M(0.f);

	mat4x4 MR(0.f);
	float cosR = cos(rpy[0]);
	float sinR = sin(rpy[0]);
	MR.val(2,2) = 1.f;
	MR.val(0,0) =  cosR;
	MR.val(0,1) = -sinR;
	MR.val(1,0) =  sinR;
	MR.val(1,1) =  cosR;

	mat4x4 MP(0.f);
	float cosP = cos(rpy[1]);
	float sinP = sin(rpy[1]);
	MP.val(0,0) = 1.f;
	MP.val(1,1) =  cosP;
	MP.val(1,2) = -sinP;
	MP.val(2,1) =  sinP;
	MP.val(2,2) =  cosP;

	mat4x4 MY(0.f);
	float cosY = cos(rpy[2]);
	float sinY = sin(rpy[2]);
	MY.val(1,1) = 1.f;
	MY.val(0,0) =  cosY;
	MY.val(0,2) =  sinY;
	MY.val(2,0) = -sinY;
	MY.val(2,2) =  cosY;

	M = MY * MP * MR;
	M.val(3,3) = 1.f;
	return M;
}

mat4x4 scaleMatrix4x4(float scale)
{
	mat4x4 M(0.f);
	for (uint32_t i = 0; i < 3; ++i)
		M.val(i,i) = scale;
	M.val(3,3) = 1.f;
	return M;
}

mat4x4 scaleMatrix4x4(const vec3& scale)
{
	mat4x4 M(0.f);
	for (uint32_t i = 0; i < 3; ++i)
		M.val(i,i) = scale[i];
	M.val(3,3) = 1.f;
	return M;
}
