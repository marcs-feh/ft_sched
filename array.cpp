
template<typename T, int N>
struct Array {
	T _v[N];

	#define ELEM_WISE_OP(op, ret) constexpr auto operator op (Array<T, N> const& x) const { \
		Array<ret, N> res; \
		for(int i = 0; i < N; ++i) res[i] = _v[i] op x._v[i]; \
		return res; \
	}

	#define ELEM_UNARY_OP(op, ret) constexpr auto operator op () const { \
		Array<ret, N> res; \
		for(int i = 0; i < N; ++i) res[i] = op _v[i]; \
		return res; \
	}

	#define X(op) ELEM_WISE_OP(op, T)
		X(+) X(-) X(*) X(/) X(%) X(&) X(|) X(^)
	#undef X

	#define X(op) ELEM_UNARY_OP(op, T)
		X(+) X(-) X(~)
	#undef X

	#define X(op) ELEM_WISE_OP(op, bool)
		X(&&) X(||) X(==) X(!=) X(>=) X(<=) X(<) X(>)
	#undef X
		
	#define X(op) ELEM_UNARY_OP(op, bool)
		X(!)
	#undef X

	#undef ELEM_WISE_OP
	#undef ELEM_UNARY_OP

};