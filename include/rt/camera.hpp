#ifndef RT_CAMERA_HPP
#define RT_CAMERA_HPP

#include <rt/vector.hpp>
#include <rt/math.hpp>
#include <rt/ray.hpp>

class Camera
{

public:

        vec3 pos;
        vec3 dir;
        vec3 right;
        vec3 up;

/*Constructor*/
	Camera(const vec3& Position, /*The position of the camera*/
	       const vec3& Direction, /*The viewing direction of the camera*/ 
	       const vec3& Up, /*Camera Up direction*/
	       float FOV, /*Angle between the top and 
			    bottom image planes (in radians)*/
	       float Aspect, /*The aspect ratio of the image (width/height)*/
	       bool RightHanded=false); /*Righthandedness of the 
					  coordinate system of the camera*/
	
/* Direction of a ray passing through a point on the image plane */
/*Sample location in NDC space. NDC space ranges from 0-1 on 
  each axis over the length of the screen with 0,0 at the top-left*/
        
        vec3 get_ray_direction(const float& xPosNDC, 
			       const float& yPosNDC) const;

        Ray  get_ray(const float& xPosNDC, 
		     const float& yPosNDC) const;
	
};

#endif /* RT_CAMERA_HPP */
