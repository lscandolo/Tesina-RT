#include <rt/camera.hpp>

Camera::Camera()
{
}

Camera::Camera(const vec3& Position, /*The position of the camera*/
	       const vec3& Direction, /*The viewing direction of the camera*/ 
	       const vec3& Up, /*Camera Up direction*/
	       float FOV, /*Angle between the top and 
			    bottom image planes (in radians)*/
	       float Aspect, /*The aspect ratio of the image (width/height)*/
	       bool RightHanded) /*Righthandedness of the 
				   coordinate system of the camera*/
	: pos(Position), dir(Direction.normalized()), right_handed(RightHanded), 
	  fov(FOV), aspect(Aspect)
{
	right = right_handed ? cross(dir,Up) : cross(Up,dir);
	up =    right_handed ? cross(right,dir) : cross(dir,right);

	/*Distance from center of viewing plane to top*/
	/*Assuming a hither distance of 1, it is: tan(fov/2)*/
	float PlaneHeight = tan(fov * 0.5f);
            
	/*Normalize basis vectors and rescale them 
	  according to FOV and aspect ratio*/
	right *= (PlaneHeight*aspect) / right.norm();
	up    *=  PlaneHeight / up.norm();
}

void Camera::set(const vec3& Position, /*The position of the camera*/
		 const vec3& Direction, /*The viewing direction of the camera*/ 
		 const vec3& Up, /*Camera Up direction*/
		 float FOV, /*Angle between the top and 
			      bottom image planes (in radians)*/
		 float Aspect, /*The aspect ratio of the image (width/height)*/
		 bool RightHanded) /*Righthandedness of the 
				   coordinate system of the camera*/
{
	pos = Position;
	dir = Direction.normalized();
	right_handed = RightHanded;
	if (FOV > 0.f) 
            fov = FOV;
    if (Aspect > 0.f)
            aspect = Aspect;

	right = right_handed ? cross(dir,Up) : cross(Up,dir);
	up =    right_handed ? cross(right,dir) : cross(dir,right);

	/*Distance from center of viewing plane to top*/
	/*Assuming a hither distance of 1, it is: tan(fov/2)*/
	float PlaneHeight = tan(FOV * 0.5f);
            
	/*Normalize basis vectors and rescale them 
	  according to FOV and aspect ratio*/
	right *= (PlaneHeight*aspect) / right.norm();
	up    *=  PlaneHeight / up.norm();
}


/* dy: radians to turn ... maybe I should switch to good ol' quaternions... */
void Camera::modifyYaw(float dy)
{
	dir = dir * cos(dy) + right.normalized() * sin(dy);
	dir.normalize();

	// dir = (dir + r_norm*dy).normalized();
	right = right_handed ? cross(dir,up) : cross(up,dir);

	/*Distance from center of viewing plane to top*/
	/*Assuming a hither distance of 1, it is: tan(fov/2)*/
	float PlaneHeight = tan(fov * 0.5f);
            
	/*Normalize basis vectors and rescale them 
	  according to FOV and aspect ratio*/
	right *= (PlaneHeight*aspect) / right.norm();
}


void Camera::modifyRoll(float dr)
{

	up = up.normalized() * cos(dr) + right.normalized() * sin(dr);
	
	right = right_handed ? cross(dir,up) : cross(up,dir);

	/*Distance from center of viewing plane to top*/
	/*Assuming a hither distance of 1, it is: tan(fov/2)*/
	float PlaneHeight = tan(fov * 0.5f);
            
	/*Normalize basis vectors and rescale them 
	  according to FOV and aspect ratio*/
	right *= (PlaneHeight*aspect) / right.norm();
	up    *=  PlaneHeight / up.norm();
}

void Camera::modifyPitch(float dp)
{
	dir = dir * cos(dp) + up.normalized() * sin(dp);
	dir.normalize();
	
	up =    right_handed ? cross(right,dir) : cross(dir,right);
	right = right_handed ? cross(dir,up) : cross(up,dir);

	/*Distance from center of viewing plane to top*/
	/*Assuming a hither distance of 1, it is: tan(fov/2)*/
	float PlaneHeight = tan(fov * 0.5f);
            
	/*Normalize basis vectors and rescale them 
	  according to FOV and aspect ratio*/
	right *= (PlaneHeight*aspect) / right.norm();
	up    *=  PlaneHeight / up.norm();
}

//!! Error conditions (non normalizable vals) need to be checked !!
void Camera::modifyAbsYaw(float dy)
{
	if (fabs(right[0]) < 0.00001f && fabs(right[1]) < 0.00001f)
		return;

	if (fabs(dir[0]) < 0.00001f && fabs(dir[1]) < 0.00001f)
		return;

	vec3 abs_right = right;
	abs_right[1] = 0.f;

	vec3 abs_dir = dir;
	abs_dir[1] = 0.f;
	float abs_dir_norm = abs_dir.norm();

	vec3 abs_up = up;
	abs_up[1] = 0.f;
	float abs_up_norm = abs_up.norm();

	abs_dir.normalize();
	abs_right.normalize();

	abs_dir = abs_dir * cos(dy) + abs_right * sin(dy);

	// This will actually make the camera roll = 0
	abs_up = -abs_dir;
	abs_up *= abs_up_norm;
	up[0] = abs_up[0]; up[2] = abs_up[2];

	abs_dir *= abs_dir_norm;
	dir[0] = abs_dir[0]; dir[2] = abs_dir[2];

	right = right_handed ? cross(dir,up) : cross(up,dir);

	/*Distance from center of viewing plane to top*/
	/*Assuming a hither distance of 1, it is: tan(fov/2)*/
	float PlaneHeight = tan(fov * 0.5f);
            
	/*Normalize basis vectors and rescale them 
	  according to FOV and aspect ratio*/
	right *= (PlaneHeight*aspect) / right.norm();
	up    *=  PlaneHeight / up.norm();
}


void Camera::panForward(float dp)
{
	pos = pos + dir*dp;
}
void Camera::panRight(float dp)
{
	pos = pos + right*dp;

}
void Camera::panUp(float dp)
{
	pos = pos + up*dp;
}


/* Direction of a ray passing through a point on the image plane */
/*Sample location in NDC space. NDC space ranges from 0-1 on 
  each axis over the length of the screen with 0,0 at the bottom-left*/

vec3
Camera::getRayDirection(const float& xPosNDC, 
			       const float& yPosNDC) const
{
	return dir + right * (xPosNDC * 2.0f - 1.0f) + 
	             up    * (yPosNDC * 2.0f - 1.0f);
}

ray_cl
Camera::getRay(const float& xPosNDC, 
		    const float& yPosNDC) const
{
	vec3 offset = right * (xPosNDC * 2.0f - 1.0f) + 
		      up    * (yPosNDC * 2.0f - 1.0f);

	return ray_cl(pos,dir+offset);
}

