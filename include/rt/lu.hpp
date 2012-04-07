#ifndef RT_LU_HPP
#define RT_LU_HPP

#include <rt/matrix.hpp>
#include <iostream> 

#define EPS 1e-5f


template <int N, typename F>
sqmat<N-1,F> removeRowCol(const sqmat<N,F>& M, int32_t r, int32_t c)
{
        sqmat<N-1,F> retmat;
        uint32_t col = 0;
        for (int32_t i = 0; i < N; ++i) { 
                if (i == c)
                        continue;
                vec<N,F> coli = M.col(i);
                vec<N-1,F> new_coli;

                for (int32_t j = 0; j < N-1; ++j)
                        if (j < r)
                                new_coli[j] = coli[j];
                        else
                                new_coli[j] = coli[j+1];
                retmat.set_col(col,new_coli);
                col++;
        }
        return retmat;
} 

template <int N, typename F>
void printMat(const sqmat<N,F>& M)
{
        for (int32_t i = 0; i < N; ++i){
                for (int32_t j = 0; j < N; ++j){
                        std::cout << M.val(i,j) << "\t";
                }
                std::cout << std::endl;
        }
}

template <typename F>
F determinant(const sqmat<1,F>& M)
{
        return M.val(0,0);
} 

template <typename F>
F determinant(const sqmat<2,F>& M)
{
        return M.val(0,0) * M.val(1,1) - M.val(0,1) * M.val(1,0);
} 

template <int N, typename F>
F determinant(const sqmat<N,F>& M)
{
        float sign = 1.f;
        float det = 0.f;
        for (uint32_t i = 0; i < N; ++i) {
                det += sign * M.val(0,i) * determinant(removeRowCol(M,0,i));
                sign *= -1.f;
        }
        return det;
}

template <int N, typename F>
sqmat<N,F> inverse(const sqmat<N,F>& M)
{
        float sign = 1.f;
        float det = 0.f;
        sqmat<N,F> inv;
        for (uint32_t i = 0; i < N; ++i) {
                for (uint32_t j = 0; j < N; ++j) {
                        sign = (i+j)%2 ? -1.f:1.f;
                        float d = determinant(removeRowCol(M,i,j));
                        if (i == 0)
                                det += sign * M.val(i,j) * d;
                        // A^{-1} = 1/det(A) * adj(A)^T
                        inv.val(j,i) = sign * d;
                }
        }

        if (fabs(det) < EPS)
                det = 0.f;
        else
                det = 1.f/det;

        
        return inv * det;
        
}

#endif /* RT_LU_HPP */
