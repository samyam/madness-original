//
// Ghaly
//
// Compresses the function, transforming into wavelet basis. 
// Possible non-blocking comm.
//
// By default fence=true meaning that this oepration completes before returning,
// othewise if fence=false it returns without fencing and the user must invoke 
// workd.gop.fence() to assure global completion before using the function
// for other purposes.
//
// Noop if already compressed or if not initialized.
//
// Since reconstruction/compression do not discard information we define them
// as const ... "logical constness" not "bitwise contness".
//
#ifndef __MADNESS_MRA_FUSET_INNER_OP__INCLUDED__
#define __MADNESS_MRA_FUSET_INNER_OP__INCLUDED__

#include "PrimitiveOp.h"
#include "../mra.h"
#include "../function_common_data.h"
#include "../../world/MADworld.h"
#include "../../tensor/tensor.h"
#include "../../tensor/gentensor.h"

namespace madness 
{
	template<typename T, std::size_t NDIM>
	class AdddddOp : public PrimitiveOp<T,NDIM> 
	{
		typedef Function<T,NDIM> KTREE;
		typedef FunctionNode<T,NDIM> KNODE;	// identical to nodeT
		typedef FunctionImpl<T,NDIM> implT;
		typedef Key<NDIM> keyT;
		typedef WorldContainer<Key<NDIM>,FunctionNode<T,NDIM>> dcT;
		typedef WorldObject<FunctionImpl<T,NDIM>> woT;	///< Base class world object type
		typedef GenTensor<T> coeffT;	//Type of tensor used to hold coeffs
		typedef Tensor<T> tensorT;
	    
    public:
		AdddddOp(string opName, KTREE* output, const KTREE* i1, const KTREE*i2);
		FuseTContainer<T> compute(const keyT& key, const FuseTContainer<T> &s);

		bool notEmpty(map<int,bool>& notEmptyMap) const{
		    unsigned long treeID = _i1->get_impl()->id().get_obj_id();
		    unsigned long treeID2 = _i2->get_impl()->id().get_obj_id();
		    //cout<<"Checking for treeID : "<<treeID<<" and result is "<<notEmptyMap[treeID]<<endl;
		    return  notEmptyMap[treeID2] && notEmptyMap[treeID];
		}

		//
		bool isDone(const keyT& key) const;
		bool isPre() const { return true; }
		bool needsParameter() const { return false; }
		void reduce(World& world);
	public:	// for Addddd Product (specific)
		T _sum;

	private:
		//!Points to operand trees
		const KTREE* _i1, *_i2, *_result;
    
		//!Points to operand nodes of the tree
		KNODE *_t1, *_t2;

		//!Variables for CompressOp
		dcT& _coeffs_left;
		dcT& _coeffs_right;
		dcT& _coeffs_target;
		const FunctionCommonData<T,NDIM>& _cdata; 
		TensorArgs _targs;	

		// Wavelet order	
		int _k;
    };

		
	// Constructor
	// World is needed for communication in the "compute" function
    template<typename T, std::size_t NDIM>
	AdddddOp<T,NDIM>::AdddddOp(string opName, KTREE* output, const KTREE* i1, const KTREE* i2)
	: PrimitiveOp<T,NDIM>(opName, output, false,true)
		, _i1(i1)
	    , _i2(i2)
		, _result(output)
		, _cdata(FunctionCommonData<T,NDIM>::get(i1->k()))
		, _targs(i1->get_impl()->get_tensor_args())
		, _coeffs_left(i1->get_impl()->get_coeffs())
		, _coeffs_right(i2->get_impl()->get_coeffs())
		, _coeffs_target(output->get_impl()->get_coeffs())
		, _k(i1->get_impl()->get_k())
		, _sum(0.0)
	{

	    // output is itself.
	    //dependnecy Info PSI, ALPHA, DELTA,SIGMA, ID
	    this->_OpID = output->get_impl()->id().get_obj_id();
	    this->_dInfoVec.push_back(DependencyInfo<T,NDIM>(i1,true,false,false,false));
	    this->_dInfoVec.push_back(DependencyInfo<T,NDIM>(i2,true,false,false,false));
	    this->_dInfoVec.push_back(DependencyInfo<T,NDIM>(output,true,false,false,false));
	    
		woT(i1->world());
	}

	//
	template<typename T, std::size_t NDIM>
	FuseTContainer<T>
	AdddddOp<T,NDIM>::compute(const keyT& key, const FuseTContainer<T> &s)
	{
		if ( key == keyT(0) )
		{
	//		this->_result->get_impl()->reset(new implT(*this->_i1.get_impl(), this->_i1.get_pmap(), false));
		}

		// impl->gaxpy(alpha,*left.get_impl(),beta,*right.get_impl(),fence);

		// other is node
		KNODE		target_node;
		TensorArgs	targs2	= this->_targs;
		bool		isLeft	= _coeffs_left.probe(key);
		bool		isRight	= _coeffs_right.probe(key);
		T alpha = 1.0;
		T beta	= 1.0;
		
		if (isLeft == true)
		{	// Left has a node with the key
			const KNODE& node_left	= _coeffs_left.find(key).get()->second;
			coeffT coeff_left = node_left.coeff().full_tensor_copy();

			if (isRight == true)
			{	// Right also has a node with the key
				const KNODE& node_right = _coeffs_right.find(key).get()->second;
				coeffT coeff_right = node_right.coeff().full_tensor_copy();

				// coeff().gaxpy(alpha, other.coeff(), beta);
				//target_node.set_coeff(coeff_right);
			}
			else
			{	// Only Left has a node with the key
				// coeff().scale(alpha);
				//target_node.set_coeff(coeff_left);	
			}
		}
		else
		{	// only Right has a node with the key
			//const KNODE& node_right = _coeffs_right.find(key).get()->second;
			//coeffT coeff_right = coeffT(copy(node_right.coeff()), targs2);
	
			// coeff() = other.coeff()*beta (const R& beta)	
	
			//target_node.set_coeff(coeff_right);
		}


		//_coeffs_target.replace(key, target_node);
		//this->_result->get_impl()->get_coeffs().replace(key, target_node);



		FuseTContainer<T> result;
		return result;
	}

    // isDone
    template<typename T, std::size_t NDIM>
	bool 
	AdddddOp<T,NDIM>::isDone(const keyT& key) const 
	{
		bool isE1 = _i1->get_impl()->get_coeffs().probe(key);
		if(!isE1)	return isE1;

		bool isE2 = _i2->get_impl()->get_coeffs().probe(key);
		if(!isE2)	return isE2;

		bool isLeaf = !_i1->get_impl()->get_coeffs().find(key).get()->second.has_children();
		if (isLeaf)	return isLeaf;

		bool isLeaf2 = !_i2->get_impl()->get_coeffs().find(key).get()->second.has_children();
		return isLeaf2;
    }
    
    // Reduction
    template<typename T, std::size_t NDIM>
	void  
	AdddddOp<T,NDIM>::reduce(World& world){
	    world.gop.sum(_sum);	
    }
}; /*fuset*/

#endif /* __fuset_AdddddOp_h__ */
