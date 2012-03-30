#ifndef RT_MATRIX_HPP
#define RT_MATRIX_HPP

#include <stdint.h>
#include <math.h>

#include <rt/assert.hpp>
#include <rt/vector.hpp>

template <int N, typename F>
class sqmat
{
public:

        /* Col major order */
        F v[N*N];

	inline sqmat<N,F> () {}

	inline sqmat<N,F> (const F& val)
		{
			for (uint32_t i = 0; i < N*N; ++i) 
				v[i] = val;
		}

	/* Row first constructor */
	inline sqmat<N,F> (const F* vals)
		{
			for (uint32_t i = 0; i < N*N; ++i) 
				v[i] = vals[i];
		}


	inline F& val(uint32_t row, uint32_t col)
		{
			return v[col*N+row];
		}

	inline F val(uint32_t row, uint32_t col) const
		{
			return v[col*N+row];
		}

	inline sqmat<N,F> operator*(const F x) const
		{
			sqmat<N,F> res(0.f);
			for(uint32_t i = 0; i < N; ++i) 
				for (uint32_t j = 0; j < N; ++j) 
                                        res.val(i,j) = val(i,j) * x;

			return res;
		}

	inline sqmat<N,F> operator*(const sqmat<N,F>& rhs) const
		{
			sqmat<N,F> res(0.f);
			for(uint32_t i = 0; i < N; ++i) {
				for (uint32_t j = 0; j < N; ++j) {
					for (uint32_t k = 0; k < N; ++k) {
						res.val(i,j) += val(i,k) * rhs.val(k,j);
					}
				}
			}
			return res;
		}

	inline sqmat<N,F> operator+(const sqmat<N,F>& rhs) const
		{
			sqmat<N,F> res(0.f);
			for(uint32_t i = 0; i < N; ++i) {
				for (uint32_t j = 0; j < N; ++j) {
					res.val(i,j) = val(i,j) + rhs.val(i,j);
				}
			}
			return res;
		}

	inline sqmat<N,F> operator-(const sqmat<N,F>& rhs) const
		{
			sqmat<N,F> res(0.f);
			for(uint32_t i = 0; i < N; ++i) {
				for (uint32_t j = 0; j < N; ++j) {
					res.val(i,j) = val(i,j) - rhs.val(i,j);
				}
			}
			return res;
		}

	inline sqmat<N,F> operator-() const
		{
			sqmat<N,F> res(0.f);
			for(uint32_t i = 0; i < N; ++i) {
				for (uint32_t j = 0; j < N; ++j) {
					res.val(i,j) = -val(i,j);
				}
			}
			return res;
		}

	inline vec<N,F> operator*(const vec<N,F>& rhs) const
		{

			vec<N,F> res(0.f);
			for(uint32_t i = 0; i < N; ++i) {
				for (uint32_t j = 0; j < N; ++j) {
					res[i] += val(i,j) * rhs[j];
				}
			}
			return res;
		}


        inline vec<N,F> row(const int n) const{
                ASSERT(n<N);
                vec<N,F> row;
                int v_idx = n;
                for (uint32_t i = 0; i < N; ++i, v_idx+=N)
                        row[i] = v[v_idx];
                return row;
        }

        inline vec<N,F> col(const int n) const{
                ASSERT(n<N);
                vec<N,F> col;
                int v_idx = n*N;
                for (uint32_t i = 0; i < N; ++i, v_idx++)
                        col[i] = v[v_idx];
                return col;
        }

        inline void set_row(const int n, const vec<N,F>& row){
                F* vptr = v+n;
                for (uint32_t i = 0; i < N; ++i, vptr+=N)
                         *vptr = row[i];
        }

        inline void set_col(const int n, const vec<N,F>& col){
                F* vptr = v+(n*N);
                for (uint32_t i = 0; i < N; ++i, vptr++)
                        *vptr = col[i];
        }
};

typedef sqmat<3,float> mat3x3;
typedef sqmat<4,float> mat4x4;


#endif /* RT_MATRIX_HPP */
