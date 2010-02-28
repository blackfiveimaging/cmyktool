#ifndef NAIVECMYKTRANSFORM_H
#define NAIVECMYKTRANSFORM_H

#include "imagesource.h"
#include "lcmswrapper.h"

class NaiveCMYKTransform : public CMSTransform
{
	public:
	NaiveCMYKTransform(ImageSource *src) : CMSTransform()
	{
		inputtype=src->type;
		outputtype=IS_TYPE_CMYK;
	}
	virtual void Transform(unsigned short *src,unsigned short *dst,int pixels)
	{
		switch(inputtype)
		{
			case IS_TYPE_RGB:
				for(int x=0;x<pixels;++x)
				{
					unsigned int c=IS_SAMPLEMAX-src[x*3];
					unsigned int m=IS_SAMPLEMAX-src[x*3+1];
					unsigned int y=IS_SAMPLEMAX-src[x*3+2];
					unsigned int k=c;
					if(m<k) k=m;
					if(y<k) k=y;
					dst[x*4+3]=k;
					dst[x*4]=c-k;
					dst[x*4+1]=m-k;
					dst[x*4+2]=y-k;
				}
				break;
			case IS_TYPE_CMYK:
				for(int x=0;x<pixels;++x)
				{
					*dst++=*src++;
					*dst++=*src++;
					*dst++=*src++;
					*dst++=*src++;
				}
				break;
			default:
				throw "NaiveCMYKTransform: Input type not yet supported";
				break;
		}
	}
};

#endif
