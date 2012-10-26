#pragma once

#include "nivision.h"
#include "extcode.h"
#include "niimaq.h"
#include <complex>

/* lv_prolog.h and lv_epilog.h set up the correct alignment for LabVIEW data. */
#include "lv_prolog.h"

typedef struct {
        LVBoolean status;
        int32 code;
        LStrHandle message;
} ErrorCluster;


template<typename T>
struct LVArray {
	int32_t dimSize;
	T elt[1];
};
typedef LVArray<float> **ppFloatArray;

template<typename T>
struct LVArray2D {
	int32_t dimSizes[2];
	T data[1];

	T & elem(int col, int row) {
		return data[row*dimSizes[0]+col];
	}

	int numElem() { return dimSizes[0]*dimSizes[1]; }
};


// Compile-time map of C++ types to Labview DataType codes
template<typename T>
struct LVDataType {};
template<> struct LVDataType<float> { enum { code=9 }; };
template<> struct LVDataType<double> { enum { code=0xa }; };
template<> struct LVDataType<int8_t> { enum { code=1 }; };
template<> struct LVDataType<int16_t> { enum { code=2 }; };
template<> struct LVDataType<int32_t> { enum { code=3 }; };
template<> struct LVDataType<int64_t> { enum { code=4 }; };
template<> struct LVDataType<uint8_t> { enum { code=5 }; };
template<> struct LVDataType<uint16_t> { enum { code=6 }; };
template<> struct LVDataType<uint32_t> { enum { code=7 }; };
template<> struct LVDataType<uint64_t> { enum { code=8 }; };
template<> struct LVDataType<std::complex<float> > { enum { code=0xc }; };
template<> struct LVDataType<std::complex<double> > { enum { code=0xd }; };

template<typename T>
void ResizeLVArray2D(LVArray2D<T>**& d, int rows, int cols) {
	NumericArrayResize(LVDataType<T>::code, 2, (UHandle*)&d, sizeof(T)*rows*cols);
	(*d)->dimSizes[0] = rows;
	(*d)->dimSizes[1] = cols;
}
template<typename T>
void ResizeLVArray(LVArray<T>**& d, int elems) {
	NumericArrayResize(LVDataType<T>::code, 1, (UHandle*)&d, sizeof(T)*elems);
	(*d)->dimSize = elems;
}

typedef LVArray2D<float> **ppFloatArray2;
#include "lv_epilog.h"

