/*
   Copyright (c) 2009-2014, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef ELEM_ADJOINT_HPP
#define ELEM_ADJOINT_HPP

#include "./Transpose.hpp"

namespace elem {

template<typename T>
inline void
Adjoint( const Matrix<T>& A, Matrix<T>& B )
{
    DEBUG_ONLY(CallStackEntry cse("Adjoint"))
    Transpose( A, B, true );
}

template<typename T,Dist U,Dist V,
                    Dist W,Dist Z>
inline void
Adjoint( const DistMatrix<T,U,V>& A, DistMatrix<T,W,Z>& B )
{
    DEBUG_ONLY(CallStackEntry cse("Adjoint"))
    Transpose( A, B, true );
}

template<typename T,Dist U,Dist V,
                    Dist W,Dist Z>
inline void
Adjoint( const BlockDistMatrix<T,U,V>& A, BlockDistMatrix<T,W,Z>& B )
{
    DEBUG_ONLY(CallStackEntry cse("Adjoint"))
    Transpose( A, B, true );
}

} // namespace elem

#endif // ifndef ELEM_ADJOINT_HPP
