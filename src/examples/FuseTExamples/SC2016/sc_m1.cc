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

  $Id$
*/
//#define WORLD_INSTANTIATE_STATIC_TEMPLATES
#include <madness/mra/mra.h>
#include <madness/mra/operator.h>
#include <madness/constants.h>
#include <madness/tensor/distributed_matrix.h>
#include <madness/tensor/cblas.h>
#include <madness/mra/FuseT/MatrixInnerOp.h>
#include <madness/mra/FuseT/InnerOp.h>
#include <madness/mra/FuseT/CompressOp.h>
#include <madness/mra/FuseT/OpExecutor.h>
#include <madness/mra/FuseT/FusedExecutor.h>
#include <madness/mra/FuseT/FuseT.h>

using namespace madness;

static const double L		= 20;     // Half box size
static const long	k		= 8;        // wavelet order
//static const double thresh	= 1e-6; // precision   // w/o diff. and 1e-12 -> 64 x 64
static const double c		= 2.0;       //
static const double tstep	= 0.1;
static const double alpha	= 1.9; // Exponent
static const double VVV		= 0.2;  // Vp constant value

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

static double random_function(const coord_3d& r) {
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

class alpha_functor : public FunctionFunctorInterface<double,3> {
private:
    double coeff;
public:
    alpha_functor(double coeff=1.0) : coeff(coeff) {}

    virtual double operator()(const coord_3d& r) const {
        const double x=r[0], y=r[1], z=r[2];
        return (coeff * (x*x + y*y + z*z) * sin(x*x + y*y + z*z));
    }
};
// Exact solution at time t
class uexact : public FunctionFunctorInterface<double,3> {
    double t;
public:
    uexact(double t) : t(t) {}

    double operator()(const coord_3d& r) const {
        const double x=r[0], y=r[1], z=r[2];
        double rsq = (x*x+y*y+z*z);

        return exp(VVV*t)*exp(-rsq*alpha/(1.0+4.0*alpha*t*c)) * pow(alpha/((1+4*alpha*t*c)*constants::pi),1.5);
    }
};

// Functor to compute exp(f) where f is a madness function
template<typename T, int NDIM>
struct unaryexp {
    void operator()(const Key<NDIM>& key, Tensor<T>& t) const {
        UNARY_OPTIMIZED_ITERATOR(T, t, *_p0 = exp(*_p0););
    }
    template <typename Archive>
    void serialize(Archive& ar) {}
};

typedef Function<double,3> functionT;
typedef std::vector<functionT> vecfuncT;

int main(int argc, char** argv) 
{
	// input
	// (1) M			-- M and M functions
	// (2) thresh		-- threshold
	// (3) max-refine	-- max-refine-level
	// (4) type			-- 0: all, 1: FuseT, 2: vmra, 3: OpExecutor

	// m1. MatrixInner : MatrixInnerOp-DGEMM (OpExecutor) vs MatrixInner-MADNESS (vmra.h) vs MatrixInner using lots of InnerOp (FusedExecutor) vs Matrix Inner using funcimpl.inner (MADNESS)
	int		max_refine_level	= 14;
	double	thresh				= 1e-06; // precision   // w/o diff. and 1e-12 -> 64 x 64
	int		FUNC_SIZE			= 32;
	int		FUNC_SIZE_M			= 32;
	int		type				= 0;

	if (argc == 5)
	{
		FUNC_SIZE			= atoi(argv[1]);
		FUNC_SIZE_M			= atoi(argv[2]);
		thresh				= atof(argv[3]);
		max_refine_level	= atoi(argv[4]);
	}

    initialize(argc, argv);
    World world(SafeMPI::COMM_WORLD);

    startup(world, argc, argv);

	if (world.rank() == 0) print ("====================================================");
	if (world.rank() == 0) printf("  Micro Benchmark #1 \n");
	if (world.rank() == 0) printf("  %d functions based on %d and %d random functions\n", FUNC_SIZE*FUNC_SIZE_M, FUNC_SIZE, FUNC_SIZE_M);
	if (world.rank() == 0) printf("  threshold: %13.4g, max_refine_level: %d\n", thresh, max_refine_level);
	if (world.rank() == 0) print ("====================================================");
	world.gop.fence();

	// Setting FunctionDefaults
    FunctionDefaults<3>::set_k(k);
    FunctionDefaults<3>::set_thresh(thresh);
    FunctionDefaults<3>::set_refine(true);
    FunctionDefaults<3>::set_autorefine(false);
    FunctionDefaults<3>::set_cubic_cell(-L, L);
	FunctionDefaults<3>::set_max_refine_level(max_refine_level);


	if (world.rank() == 0) print ("====================================================");
    if (world.rank() == 0) print ("==   Initializing Functions   ======================");
	if (world.rank() == 0) print ("====================================================");
    world.gop.fence();

	// 2 * N Functions
	real_function_3d  h[FUNC_SIZE];
	real_function_3d  g[FUNC_SIZE_M];
	real_function_3d  output[FUNC_SIZE*FUNC_SIZE_M];

	real_function_3d  result_factory(world);
	real_function_3d  result(result_factory);

	int i, j;
	double clkbegin, clkend;
	clkbegin = rtclock();

	for (i=0; i<FUNC_SIZE; i++) 
	{
		randomizer();
		h[i]				= real_factory_3d(world).f(random_function);
	}

	for (i=0; i<FUNC_SIZE_M; i++)
	{
		randomizer();
		g[i]				= real_factory_3d(world).f(random_function);
	}

	for (i=0; i<FUNC_SIZE; i++)
		for (j=0; j<FUNC_SIZE_M; j++)
			output[i*FUNC_SIZE_M + j] = h[i]*g[j];

	// m1. MatrixInner : MatrixInnerOp-DGEMM (OpExecutor) vs MatrixInner-MADNESS (vmra.h) vs MatrixInner using lots of InnerOp (FusedExecutor) vs Matrix Inner using funcimpl.inner (MADNESS)
	clkend = rtclock() - clkbegin;
	if (world.rank() == 0) printf("Running Time: %f\n", clkend);

	clkbegin = rtclock();
	for (i=0; i<FUNC_SIZE; i++)
		for (j=0; j<FUNC_SIZE_M; j++) {
			output[i*FUNC_SIZE_M + j].compress(); 
			output[i*FUNC_SIZE_M + j].truncate();
		}

	clkend = rtclock() - clkbegin;
	if (world.rank() == 0) printf("Running Time-- compress(): %f\n", clkend);
	world.gop.fence();


	if (world.rank() == 0) print ("====================================================");
	if (world.rank() == 0) print ("=== MatrixInnerOp-DGEMM (OpExecutor) ===============");
	if (world.rank() == 0) print ("====================================================");
	world.gop.fence();

	vecfuncT fs;
	vecfuncT gs;

	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++)						fs.push_back(output[i]);
	for (i=FUNC_SIZE*FUNC_SIZE_M/2; i<FUNC_SIZE*FUNC_SIZE_M; i++)	gs.push_back(output[i]);

	clkbegin = rtclock();

	MatrixInnerOp<double,3>*	matrix_inner_op = new MatrixInnerOp<double, 3>("MatrixInner", &result, fs, gs, false, false);
	OpExecutor<double,3>		exe(world);
	exe.execute(matrix_inner_op, false);

	clkend = rtclock() - clkbegin;
	if (world.rank() == 0)	printf("Running Time: %f\n", clkend);
	world.gop.fence();
/*
	if (world.rank() == 0)
	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++)
		for (j=0; j<FUNC_SIZE_M*FUNC_SIZE_M/2; j++)	
		{	
			printf ("(%d,%d): %f\n", i, j, (*matrix_inner_op->_r)(i, j));
		}
	world.gop.fence();
*/

//
//
//

	// m1. MatrixInner : MatrixInnerOp-DGEMM (OpExecutor) vs MatrixInner-MADNESS (vmra.h) vs MatrixInner using lots of InnerOp (FusedExecutor) vs Matrix Inner using funcimpl.inner (MADNESS)
	if (world.rank() == 0) print ("====================================================");
	if (world.rank() == 0) print ("=== MatrixInner-MADNESS (vmra.h) ===================");
	if (world.rank() == 0) print ("====================================================");
	world.gop.fence();

	vecfuncT v_f;
	vecfuncT v_g;

	clkbegin = rtclock();
	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++)
		v_f.push_back(output[i]);

	for (i=FUNC_SIZE*FUNC_SIZE_M/2; i<FUNC_SIZE*FUNC_SIZE_M; i++)
		v_g.push_back(output[i]);

	Tensor<double> ghaly = matrix_inner(world, v_f, v_g);

	clkend = rtclock() - clkbegin;
	if (world.rank() == 0)	printf("Running Time: %f\n", clkend);
	world.gop.fence();

/*
	if (world.rank() == 0)
	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++) {
		for (j=0; j<FUNC_SIZE_M*FUNC_SIZE/2; j++){
			printf ("matrix_inner_old: r(%d,%d): %f\n", i, j, ghaly(i, j));
		}
	}
	world.gop.fence();
*/
//
//
//
	// m1. MatrixInner : MatrixInnerOp-DGEMM (OpExecutor) vs MatrixInner-MADNESS (vmra.h) vs MatrixInner using lots of InnerOp (FusedExecutor) vs Matrix Inner using funcimpl.inner (MADNESS)
	if (world.rank() == 0) print ("====================================================");
	if (world.rank() == 0) print ("=== MatrixInner using InnerOp (FusedExecutor) ======");
	if (world.rank() == 0) print ("====================================================");
	world.gop.fence();

	clkbegin = rtclock();
	InnerOp<double,3>*	inner_op_ug[FUNC_SIZE*FUNC_SIZE_M/2][FUNC_SIZE_M*FUNC_SIZE/2];

	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++)
		for (j=0; j<FUNC_SIZE_M*FUNC_SIZE/2; j++)
			inner_op_ug[i][j] = new InnerOp<double,3>("Inner",&result,&output[i],&output[(FUNC_SIZE*FUNC_SIZE_M/2) + j]);

	vector<PrimitiveOp<double,3>*>	sequence;

	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++)
		for (j=0; j<FUNC_SIZE_M*FUNC_SIZE/2; j++)
			sequence.push_back(inner_op_ug[i][j]);

	FuseT<double,3> odag(sequence);
	odag.processSequence();

	FusedOpSequence<double,3> fsequence = odag.getFusedOpSequence();
	FusedExecutor<double,3> fexecuter(world, &fsequence);
	fexecuter.execute();

	clkend = rtclock() - clkbegin;
	if (world.rank() == 0)	printf("Running Time: %f\n", clkend);
	world.gop.fence();

/*
	if (world.rank() == 0)
	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++) {
		for (j=0; j<FUNC_SIZE_M*FUNC_SIZE/2; j++){
			printf ("inner fused: r(%d,%d): %f\n", i, j, inner_op_ug[i][j]->_sum);
		}
	}
	world.gop.fence();
*/
/*
	// m1. MatrixInner : MatrixInnerOp-DGEMM (OpExecutor) vs MatrixInner-MADNESS (vmra.h) vs MatrixInner using lots of InnerOp (FusedExecutor) vs Matrix Inner using funcimpl.inner (MADNESS)
	if (world.rank() == 0) print ("====================================================");
	if (world.rank() == 0) print ("=== MatrixInner using funcimpl.inner (MADNESS) =====");
	if (world.rank() == 0) print ("====================================================");
	world.gop.fence();

	double resultInner[FUNC_SIZE*FUNC_SIZE_M/2][FUNC_SIZE_M*FUNC_SIZE/2];

	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++)
		for (j=0; j<FUNC_SIZE*FUNC_SIZE_M/2; j++)
			resultInner[i][j] = 0.0;

	clkbegin = rtclock();

	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++)
		for (j=0; j<FUNC_SIZE_M*FUNC_SIZE/2; j++) 
			resultInner[i][j] = output[i].inner(output[(FUNC_SIZE*FUNC_SIZE_M/2) + j]); 

	clkend = rtclock() - clkbegin;
	if (world.rank() == 0)	printf("Running Time: %f\n", clkend);
	world.gop.fence();
*/
/*
	if (world.rank() == 0)
	for (i=0; i<FUNC_SIZE*FUNC_SIZE_M/2; i++) {
		for (j=0; j<FUNC_SIZE_M*FUNC_SIZE/2; j++){
			printf ("inner: r(%d,%d): %f\n", i, j, resultInner[i][j]);
		}
	}
	world.gop.fence();
*/
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

