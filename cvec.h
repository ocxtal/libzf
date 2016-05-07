
/**
 * @file cvec.h
 *
 * @brief char vector operations
 */
#ifndef _CVEC_H_INCLUDED
#define _CVEC_H_INCLUDED


/* x86_64 */
#ifdef __x86_64__

#include <immintrin.h>


#ifdef __POPCNT__
	#define _cv_popcnt(x)		_mm_popcnt_u64(x)
#else
	static inline
	int _cv_popcnt(uint64_t n)
	{
		uint64_t c = 0;
		c = (n & 0x5555555555555555) + ((n>>1) & 0x5555555555555555);
		c = (c & 0x3333333333333333) + ((c>>2) & 0x3333333333333333);
		c = (c & 0x0f0f0f0f0f0f0f0f) + ((c>>4) & 0x0f0f0f0f0f0f0f0f);
		c = (c & 0x00ff00ff00ff00ff) + ((c>>8) & 0x00ff00ff00ff00ff);
		c = (c & 0x0000ffff0000ffff) + ((c>>16) & 0x0000ffff0000ffff);
		c = (c & 0x00000000ffffffff) + ((c>>32) & 0x00000000ffffffff);
		return(c);
	}
#endif


#ifdef __BMI__
/** immintrin.h is already included */
	#define _cv_tzcnt(x)		_tzcnt_u64(x)
#else
	static inline
	int _cv_tzcnt(uint64_t n)
	{
		n |= n<<1;
		n |= n<<2;
		n |= n<<4;
		n |= n<<8;
		n |= n<<16;
		n |= n<<32;
		return(64-_cv_popcnt(n));
	}
#endif



#  if defined(__AVX2__)

/**
 *
 * x86_64 AVX2
 *
 */
#define CVEC_VECTOR_DEFINED			1
#include <smmintrin.h>

typedef struct cvec_s {
	__m256i v1;
} cvec_t;

/* expanders (without argument) */
#define _e_x_cvec_1(u)

/* expanders (without immediate) */
#define _e_v_cvec_1(a)				(a).v1
#define _e_vv_cvec_1(a, b)			(a).v1, (b).v1
#define _e_vvv_cvec_1(a, b, c)		(a).v1, (b).v1, (c).v1

/* expanders with immediate */
#define _e_i_cvec_1(imm)			(imm)
#define _e_vi_cvec_1(a, imm)		(a).v1, (imm)
#define _e_vvi_cvec_1(a, b, imm)	(a).v1, (b).v1, (imm)

/* address calculation macros */
#define _addr_cvec_1(imm)			( (__m256i *)(imm) )
#define _pv_cvec(ptr)				( _addr_cvec_1(ptr) )
/* expanders with pointers */
#define _e_p_cvec_1(ptr)			_addr_cvec_1(ptr)
#define _e_pv_cvec_1(ptr, a)		_addr_cvec_1(ptr), (a).v1

/* expand intrinsic name */
#define _i_cvec(intrin) 			_mm256_##intrin##_epi8
#define _i_cvecx(intrin)			_mm256_##intrin##_si256

/* apply */
#define _a_cvec(intrin, expander, ...) ( \
	(cvec_t) { \
		_i_cvec(intrin)(expander##_cvec_1(__VA_ARGS__)) \
	} \
)
#define _a_cvecx(intrin, expander, ...) ( \
	(cvec_t) { \
		_i_cvecx(intrin)(expander##_cvec_1(__VA_ARGS__)) \
	} \
)
#define _a_cvecxv(intrin, expander, ...) { \
	_i_cvecx(intrin)(expander##_cvec_1(__VA_ARGS__)); \
}


/* load and store (always unaligned) */
#define _load_cvec(...)		_a_cvecx(loadu, _e_p, __VA_ARGS__)
#define _store_cvec(...)	_a_cvecxv(storeu, _e_pv, __VA_ARGS__)

/* broadcast */
#define _set_cvec(...)		_a_cvec(set1, _e_i, __VA_ARGS__)
#define _zero_cvec()		_a_cvecx(setzero, _e_x, _unused)

/* logics */
#define _not_cvec(...)		_a_cvecx(not, _e_v, __VA_ARGS__)
#define _and_cvec(...)		_a_cvecx(and, _e_vv, __VA_ARGS__)
#define _or_cvec(...)		_a_cvecx(or, _e_vv, __VA_ARGS__)
#define _xor_cvec(...)		_a_cvecx(xor, _e_vv, __VA_ARGS__)
#define _andn_cvec(...)		_a_cvecx(andnot, _e_vv, __VA_ARGS__)

/* '\0' detection */
#define _null_cvec(v) 		( _mask_cvec(v, _zero_cvec()) )
#define _strlen_cvec(v)		( _tzcnt_cvec(_null_cvec()) )

/* alphabet conversion: 5bit uint -> ascii */
#define _conv_5a_cvec(v) 	( _or_cvec(_set_cvec(0x60), v) )
#define _conv_5A_cvec(v)	( _or_cvec(_set_cvec(0x40), v) )

/* ascii -> 5bit uint */
#define _conv_a5_cvec(v)	( _and_cvec(_set_cvec(0x1f), v) )

/* alphabet conversion with table */
#define _shuf_cvec(...)		_a_cvec(shuffle, _e_vv, __VA_ARGS__)
#define _gt_cvec(...)		_a_cvec(cmpgt, _e_vv, __VA_ARGS__)
#define _sel_cvec(...)		_a_cvec(blendv, _e_vvv, __VA_ARGS__)
#define _conv_4t_cvec(v, pt) ({ \
	__m128i const _t = _mm_loadu_si128((__m128i *)(pt)); \
	cvec_t _r = _shuf_cvec(((cvec_t){ _t, _t }), (v)); \
	_r; \
})
#define _conv_5t_cvec(v, pt) ({ \
	__m256i const _t1 = _mm256_broadcastsi128_si256( \
		_mm_loadu_si128((__m128i *)(pt))); \
	__m156i const _t1 = _mm256_broadcastsi128_si256( \
		_mm_loadu_si128((__m128i *)(pt) + 1)); \
	cvec_t _r1 = _shuf_cvec(((cvec_t){ _t1 }), (v)); \
	cvec_t _r2 = _shuf_cvec(((cvec_t){ _t2 }), (v)); \
	cvec_t _mask = _gt_cvec((v), _set_cvec(0x10)); \
	cvec_t r = _sel_cvec(_r1, _r2, _mask); \
	r; \
})



#  elif defined(__SSE4_1__)

/**
 *
 * x86_64 SSE4.1
 *
 */
#define CVEC_VECTOR_DEFINED			1
#include <smmintrin.h>

typedef struct cvec_s {
	__m128i v1, v2;
} cvec_t;

/* expanders (without argument) */
#define _e_x_cvec_1(u)
#define _e_x_cvec_2(u)

/* expanders (without immediate) */
#define _e_v_cvec_1(a)				(a).v1
#define _e_v_cvec_2(a)				(a).v2
#define _e_vv_cvec_1(a, b)			(a).v1, (b).v1
#define _e_vv_cvec_2(a, b)			(a).v2, (b).v2
#define _e_vvv_cvec_1(a, b, c)		(a).v1, (b).v1, (c).v1
#define _e_vvv_cvec_2(a, b, c)		(a).v2, (b).v2, (c).v2

/* expanders with immediate */
#define _e_i_cvec_1(imm)			(imm)
#define _e_i_cvec_2(imm)			(imm)
#define _e_vi_cvec_1(a, imm)		(a).v1, (imm)
#define _e_vi_cvec_2(a, imm)		(a).v2, (imm)
#define _e_vvi_cvec_1(a, b, imm)	(a).v1, (b).v1, (imm)
#define _e_vvi_cvec_2(a, b, imm)	(a).v2, (b).v2, (imm)

/* address calculation macros */
#define _addr_cvec_1(imm)			( (__m128i *)(imm) )
#define _addr_cvec_2(imm)			( (__m128i *)(imm) + 1 )
#define _pv_cvec(ptr)				( _addr_cvec_1(ptr) )
/* expanders with pointers */
#define _e_p_cvec_1(ptr)			_addr_cvec_1(ptr)
#define _e_p_cvec_2(ptr)			_addr_cvec_2(ptr)
#define _e_pv_cvec_1(ptr, a)		_addr_cvec_1(ptr), (a).v1
#define _e_pv_cvec_2(ptr, a)		_addr_cvec_2(ptr), (a).v2

/* expand intrinsic name */
#define _i_cvec(intrin) 			_mm_##intrin##_epi8
#define _i_cvecx(intrin)			_mm_##intrin##_si128

/* apply */
#define _a_cvec(intrin, expander, ...) ( \
	(cvec_t) { \
		_i_cvec(intrin)(expander##_cvec_1(__VA_ARGS__)), \
		_i_cvec(intrin)(expander##_cvec_2(__VA_ARGS__)) \
	} \
)
#define _a_cvecx(intrin, expander, ...) ( \
	(cvec_t) { \
		_i_cvecx(intrin)(expander##_cvec_1(__VA_ARGS__)), \
		_i_cvecx(intrin)(expander##_cvec_2(__VA_ARGS__)) \
	} \
)
#define _a_cvecxv(intrin, expander, ...) { \
	_i_cvecx(intrin)(expander##_cvec_1(__VA_ARGS__)); \
	_i_cvecx(intrin)(expander##_cvec_2(__VA_ARGS__)); \
}


/* load and store (always unaligned) */
#define _load_cvec(...)		_a_cvecx(loadu, _e_p, __VA_ARGS__)
#define _store_cvec(...)	_a_cvecxv(storeu, _e_pv, __VA_ARGS__)

/* broadcast */
#define _set_cvec(...)		_a_cvec(set1, _e_i, __VA_ARGS__)
#define _zero_cvec()		_a_cvecx(setzero, _e_x, _unused)

/* logics */
#define _not_cvec(...)		_a_cvecx(not, _e_v, __VA_ARGS__)
#define _and_cvec(...)		_a_cvecx(and, _e_vv, __VA_ARGS__)
#define _or_cvec(...)		_a_cvecx(or, _e_vv, __VA_ARGS__)
#define _xor_cvec(...)		_a_cvecx(xor, _e_vv, __VA_ARGS__)
#define _andn_cvec(...)		_a_cvecx(andnot, _e_vv, __VA_ARGS__)

/* '\0' detection */
#define _null_cvec(v) 		( _mask_cvec(v, _zero_cvec()) )
#define _strlen_cvec(v)		( _cv_tzcnt(_null_cvec()) )

/* alphabet conversion: 5bit uint -> ascii */
#define _conv_5a_cvec(v) 	( _or_cvec(_set_cvec(0x60), v) )
#define _conv_5A_cvec(v)	( _or_cvec(_set_cvec(0x40), v) )

/* ascii -> 5bit uint */
#define _conv_a5_cvec(v)	( _and_cvec(_set_cvec(0x1f), v) )

/* alphabet conversion with table */
#define _shuf_cvec(...)		_a_cvec(shuffle, _e_vv, __VA_ARGS__)
#define _gt_cvec(...)		_a_cvec(cmpgt, _e_vv, __VA_ARGS__)
#define _sel_cvec(...)		_a_cvec(blendv, _e_vvv, __VA_ARGS__)
#define _conv_4t_cvec(v, pt) ({ \
	__m128i const _t = _mm_loadu_si128((__m128i *)(pt)); \
	cvec_t _r = _shuf_cvec(((cvec_t){ _t, _t }), (v)); \
	_r; \
})
#define _conv_5t_cvec(v, pt) ({ \
	__m128i const _t1 = _mm_loadu_si128((__m128i *)(pt)); \
	__m128i const _t1 = _mm_loadu_si128((__m128i *)(pt) + 1); \
	cvec_t _r1 = _shuf_cvec(((cvec_t){ _t1, _t1 }), (v)); \
	cvec_t _r2 = _shuf_cvec(((cvec_t){ _t2, _t2 }), (v)); \
	cvec_t _mask = _gt_cvec((v), _set_cvec(0x10)); \
	cvec_t r = _sel_cvec(_r1, _r2, _mask); \
	r; \
})



#  else
#    error "No SIMD instruction set enabled. Check if SSE4.1 or AVX2 instructions are available and add `-msse4.1' or `-mavx2' to CFLAGS."
#  endif
#endif

/* arm 64bit */
#ifdef AARCH64
#endif

/* PPC 64bit */
#ifdef PPC64
#endif

#ifndef CVEC_VECTOR_DEFINED
#  error "No SIMD environment detected. Check CFLAGS."
#endif


#endif /* _CVEC_H_INCLUDED */
/**
 * end of cvec.h
 */
