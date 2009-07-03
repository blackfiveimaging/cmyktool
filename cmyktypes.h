#ifndef CMYK_TYPES_H
#define CMYK_TYPES_H

#include <iostream>

class CMYKValue
{
	public:
	CMYKValue() : c(0),m(0),y(0),k(0)
	{}
	CMYKValue(int c,int m,int y,int k) : c(c),m(m),y(y),k(k)
	{}
	CMYKValue(CMYKValue &v) : c(v.c), m(v.m), y(v.y), k(v.k)
	{}
	~CMYKValue()
	{}
	void Dump()
	{
		std::cerr << c << ", " << m << ", " << y << ", " << k << std::endl;
	}
	int c,m,y,k;
};

#endif
