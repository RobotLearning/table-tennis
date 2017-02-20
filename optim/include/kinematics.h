/*
 * kinematics.h
 *
 * Here we include the kinematics related functions taken from SL
 *
 *  Created on: Jun 22, 2016
 *      Author: okoc
 */

#ifndef KINEMATICS_H_
#define KINEMATICS_H_

#include "math.h"
#include "SL.h"

#define NDOF 7
#define NCART 3
#define NQUAT 4

// FROM MDEFS.H FILE
#define Power(x, y)	(pow((double)(x), (double)(y)))
#define Sqrt(x)		(sqrt((double)(x)))

#define Abs(x)		(fabs((double)(x)))

#define Exp(x)		(exp((double)(x)))
#define Log(x)		(log((double)(x)))

#define Sin(x)		(sin((double)(x)))
#define Cos(x)		(cos((double)(x)))
#define Tan(x)		(tan((double)(x)))

#define ArcSin(x)       (asin((double)(x)))
#define ArcCos(x)       (acos((double)(x)))
#define ArcTan(x)       (atan((double)(x)))

#define Sinh(x)          (sinh((double)(x)))
#define Cosh(x)          (cosh((double)(x)))
#define Tanh(x)          (tanh((double)(x)))


#ifndef E
#define E		2.71828182845904523536029
#endif
#ifndef Pi
#define Pi		3.14159265358979323846264
#endif
#define Degree		0.01745329251994329576924

// kinematics functions from SL
void calc_racket_state(const double q[NDOF],
		               const double qdot[NDOF],
					   double pos[NCART],
					   double vel[NCART],
					   double normal[NCART]);
void get_cart_velocity(Matrix jac,
		               const double qdot[NDOF],
					   double vel[NCART]);
void rev_geo_jac_col(Vector p, Vector pi, Vector zi, Vector c);

void kinematics(const double state[NDOF],
		        Matrix Xlink,
				Matrix Xorigin,
				Matrix Xaxis,
		        Matrix Ahmat[]);
void jacobian(Matrix link,
		      Matrix origin,
		      Matrix axis,
			  Matrix jac);
void set_endeffector(double endeff_pos[NCART]);

// loading joint limits from SL config files
int read_joint_limits(double *lb, double *ub);
int read_sensor_offsets(char *fname);

#endif /* KINEMATICS_H_ */
