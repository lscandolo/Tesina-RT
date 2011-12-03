#include <rt/camera.hpp>

Camera::Camera(const vec3& Position, /*The position of the camera*/
	       const vec3& Direction, /*The viewing direction of the camera*/ 
	       const vec3& Up, /*Camera Up direction*/
	       float FOV, /*Angle between the top and 
			    bottom image planes (in radians)*/
	       float Aspect, /*The aspect ratio of the image (width/height)*/
	       bool RightHanded) /*Righthandedness of the 
				   coordinate system of the camera*/
	: pos(Position), dir(normalize(Direction))
{
	right = RightHanded ? cross(dir,Up) : cross(Up,dir);
	up =    RightHanded ? cross(right,dir) : cross(dir,right);

	/*Distance from center of viewing plane to top*/
	/*Assuming a hither distance of 1, it is: tan(fov/2)*/
	float PlaneHeight = tan(FOV * 0.5f);
            
	/*Normalize basis vectors and rescale them 
	  according to FOV and aspect ratio*/
	right *= (PlaneHeight*Aspect) / norm(right);
	up    *=  PlaneHeight / norm(up);

}

/* Direction of a ray passing through a point on the image plane */
/*Sample location in NDC space. NDC space ranges from 0-1 on 
  each axis over the length of the screen with 0,0 at the top-left*/

vec3 Camera::get_ray_direction(const float& xPosNDC, 
			       const float& yPosNDC) const
{
	return dir + right * (xPosNDC * 2.0f - 1.0f) - 
	             up    * (yPosNDC * 2.0f - 1.0f);
}

Ray Camera::get_ray(const float& xPosNDC, 
		    const float& yPosNDC) const
{
	vec3 offset = right * (xPosNDC * 2.0f - 1.0f) - 
		      up    * (yPosNDC * 2.0f - 1.0f);
	return Ray(pos,dir + offset);
}
