/*
  This file is part of MADNESS.

  Copyright (C) 2007,2010 Oak Ridge National Laboratory

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

  For more information please contact:

  Robert J. Harrison
  Oak Ridge National Laboratory
  One Bethel Valley Road
  P.O. Box 2008, MS-6367

  email: harrisonrj@ornl.gov
  tel:   865-241-3937
  fax:   865-572-0680
*/


/// \file examples/hello.cc
/// \brief Simplest example program for MADNESS
/// \defgroup hellowworldmad Hello world MADNESS style
/// \ingroup examples
///
/// Simplest program that initializes the MADNESS parallel runtime
/// using initialize(), makes a madness::World object, prints
/// a greeting, and then cleans up.
///
/// To initialize the MADNESS numerical environment you also need
/// \c startup(world,argc,argv) and should include mra/mra.h rather
/// than world/MADworld.h .

#include <madness/mra/mra.h>
#include <madness/mra/operator.h>
#include <madness/constants.h>
#include <madness/mra/FuseT/InnerOp.h>
#include <madness/mra/FuseT/AddOp.h>
#include <madness/mra/FuseT/MultiplyOp.h>
#include <madness/mra/FuseT/CompressOp.h>
#include <madness/mra/FuseT/DerivativeOp.h>
#include <madness/mra/FuseT/FuseT.h>
#include <madness/mra/FuseT/FusedExecutor.h>
#include <madness/mra/FuseT/OpExecutor.h>

using namespace madness;;

static const double L		= 20;
static const long	k		= 8;
static const double thresh	= 1e-12;
static const double c		= 2.0;
static const double alpha	= 1.9; // Exponent

#define PI 3.1415926535897932385
#define LO 0.0000000000
#define HI 4.0000000000

static double sin_amp		= 1.0;
static double cos_amp		= 1.0;
static double sin_freq		= 1.0;
static double cos_freq		= 1.0;
static double sigma_x		= 1.0;
static double sigma_y		= 1.0;
static double sigma_z		= 1.0;
static double center_x		= 0.0;
static double center_y		= 0.0;
static double center_z		= 0.0;
static double gaussian_amp	= 1.0;
static double sigma_sq_x	= sigma_x*sigma_x;
static double sigma_sq_y	= sigma_y*sigma_y;
static double sigma_sq_z	= sigma_z*sigma_z;

double rtclock();

static double uinitial(const coord_3d& r) 
{
	const double x=r[0], y=r[1], z=r[2];
    return -2.0/(sqrt(x*x+y*y+z*z+1e-8));
}

static double random_function(const coord_3d& r) 
{
	const double x=r[0], y=r[1], z=r[2];

	const double dx = x - center_x;
	const double dy = y - center_y;
	const double dz = z - center_z;

	const double periodic_part = sin_amp * sin(sin_freq*(dx+dy+dz)) 
									+ cos_amp * cos(cos_freq*(dx+dy+dz));

	const double x_comp = dx*dx/sigma_sq_x;
	const double y_comp = dy*dy/sigma_sq_y;
	const double z_comp = dz*dz/sigma_sq_z;

	const double gaussian_part = -gaussian_amp/exp(sqrt(x_comp+y_comp+z_comp));

	return gaussian_part*gaussian_part;
}

static double get_rand() {
	double r3 = LO + static_cast<double>(rand())/(static_cast<double>(RAND_MAX/(HI-LO)));
	return r3;
}

static void randomizer()
{
	sin_amp = get_rand();
	cos_amp = get_rand();
	sin_freq = get_rand();
	cos_freq = get_rand();
	sigma_x = get_rand();
	sigma_y = get_rand();
	sigma_z = get_rand();
	center_x = get_rand()*L/(2.0*HI);
	center_y = get_rand()*L/(2.0*HI);
	center_z = get_rand()*L/(2.0*HI);
	gaussian_amp = get_rand();
	sigma_sq_x = sigma_x*sigma_x;
	sigma_sq_y = sigma_y*sigma_y;
	sigma_sq_z = sigma_z*sigma_z;
}

int main(int argc, char** argv) 
{
  initialize(argc,argv);
  World world(SafeMPI::COMM_WORLD);

  startup(world, argc, argv);

  FunctionDefaults<3>::set_k(k);
  FunctionDefaults<3>::set_thresh(thresh);
  FunctionDefaults<3>::set_refine(true);
  FunctionDefaults<3>::set_autorefine(false);
  FunctionDefaults<3>::set_cubic_cell(-L, L);
  FunctionDefaults<3>::set_max_refine_level(6);

  real_function_3d f1 = real_factory_3d(world).f(random_function);

  real_function_3d i0_factory = real_factory_3d(world);
  real_function_3d i1_factory = real_factory_3d(world);
  real_function_3d i2_factory = real_factory_3d(world);
  real_function_3d i0(i0_factory);
  real_function_3d i1(i1_factory);
  real_function_3d i2(i2_factory);

  f1.truncate();

  real_derivative_3d D0 = free_space_derivative<double,3>(world, 0);
  real_derivative_3d D1 = free_space_derivative<double,3>(world, 1);
  real_derivative_3d D2 = free_space_derivative<double,3>(world, 2);
  double norm2 = f1.norm2();
  double trace  = f1.trace();
  if (world.rank() == 0) print("[Input] Norm0 : ", norm2," Trace : ",trace);


double clkbegin, clkend;
  if (world.rank() == 0) print ("=====================================================");
  if (world.rank() == 0) print ("   MADNESS                                           ");
  if (world.rank() == 0) print ("=====================================================");

  clkbegin = rtclock();

  real_function_3d df0 = D0(f1);
  real_function_3d df1 = D1(f1);
  real_function_3d df2 = D2(f1);
  world.gop.fence();

  clkend = rtclock() - clkbegin;
  if (world.rank() == 0) printf("MADNESS Running Time: %f\n", clkend);

  double MADnorm0 = df0.norm2();
   double MADnorm1 = df1.norm2();
  double MADnorm2 = df2.norm2();

  double MADtrace0 = df0.trace();
  double MADtrace1 = df1.trace();
 double MADtrace2 = df2.trace();

  if (world.rank() == 0) print ("=====================================================");
  if (world.rank() == 0) print ("   FuseT                                             ");
  if (world.rank() == 0) print ("=====================================================");

  clkbegin = rtclock();

  //f1.reconstruct();
  //world.gop.fence();
  DerivativeOp<double,3> derivative_op_0("Derivative",&i0,&f1,world,&D0);		// i5 <-- i1
  DerivativeOp<double,3> derivative_op_1("Derivative",&i1,&f1,world,&D1);		// i6 <-- f3
  DerivativeOp<double,3> derivative_op_2("Derivative",&i2,&f1,world,&D2);		// i7 <-- f4


  OpExecutor<double,3> exe(world);
  exe.execute(&derivative_op_0, false);
  exe.execute(&derivative_op_1, false);
  exe.execute(&derivative_op_2, false);
  //world.gop.fence();

  clkend = rtclock() - clkbegin;
  if (world.rank() == 0) printf("OpExecutor Running Time: %f\n", clkend);

  double FuseTnorm0 = i0.norm2();
  double FuseTnorm1 = i1.norm2();
  double FuseTnorm2 = i2.norm2();

  double FuseTtrace0 = i0.trace();
  double FuseTtrace1 = i1.trace();
  double FuseTtrace2 = i2.trace();


  real_function_3d dif0 = i0 - df0;
  real_function_3d dif1 = i1 - df1;
  real_function_3d dif2 = i2 - df2;
  world.gop.fence();

  double Difnorm0 = dif0.norm2();
  double Difnorm1 = dif1.norm2();
  double Difnorm2 = dif2.norm2();


  if (world.rank() == 0) print("[Result MADNESS] Norm0 : ", MADnorm0," Trace : ", MADtrace0);
  if (world.rank() == 0) print("[Result Fuset] Norm0 : ", FuseTnorm0," Trace : ", FuseTtrace0);

  if (world.rank() == 0) print("[Result MADNESS] Norm1 : ", MADnorm1," Trace : ", MADtrace1);
  if (world.rank() == 0) print("[Result Fuset] Norm1 : ", FuseTnorm1," Trace : ", FuseTtrace1);

  if (world.rank() == 0) print("[Result MADNESS] Norm2 : ", MADnorm2," Trace : ", MADtrace2);
  if (world.rank() == 0) print("[Result Fuset] Norm2 : ", FuseTnorm2," Trace : ", FuseTtrace2);

  if (world.rank() == 0) print("[Difference] Norm2 : ", Difnorm0);
  if (world.rank() == 0) print("[Difference] Norm2 : ", Difnorm1);
  if (world.rank() == 0) print("[Difference] Norm2 : ", Difnorm2);
  

  finalize();
  return 0;
}

double rtclock()
{
	struct timezone Tzp;
    struct timeval Tp;
    int stat;
    stat = gettimeofday (&Tp, &Tzp);
    if (stat != 0)
	printf("Error return from gettimeofday: %d", stat);
    return (Tp.tv_sec + Tp.tv_usec * 1.0e-6);
}
