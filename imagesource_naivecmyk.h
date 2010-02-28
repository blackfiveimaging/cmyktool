#ifndef IMAGESOURCE_NAIVECMYK_H
#define IMAGESOURCE_NAIVECMYK_H

#include "imagesource.h"

class ImageSource_NaiveCMYK : public ImageSource
{
	ImageSource_NaiveCMYK(ImageSource *src) : ImageSource(src), source(src)
	{
		type=IS_TYPE_CMYK;
		MakeRowBuffer();
		currentrow=-1;
	}
	~ImageSource_NaiveCMYK()
	{
		if(source)
			delete source;
	}
	ISDataType *GetRow(int row)
	{
		if(row=currentrow)
			return(rowbuffer);

		ISDataType *srcrow=source->GetRow(row);

		switch(source->type)
		{
			case IS_TYPE_RGB:
				for(int x=0;x<width;++x)
				{
					unsigned int r=src[x*3];
					unsigned int g=src[x*3+1];
					unsigned int b=src[x*3+2];
					unsigned int k=r;
					if(g>k) k=g;
					if(b>k) k=b;
					r-=k;
					g-=k;
					b-=k;
					rowbuffer[x*4]=IS_SAMPLEMAX-r;
					rowbuffer[x*4+1]=IS_SAMPLEMAX-g;
					rowbuffer[x*4+2]=IS_SAMPLEMAX-b;
					rowbuffer[x*4+3]=IS_SAMPLEMAX-k;
				}
				break;
			case IS_TYPE_CMYK:
				return(srcrow);
				break;
			default:
				throw "ImageSource_NaiveCMYK: Input type not yet supported" << endl;
				break;
		}
		currentrow=row;
		return(rowbuffer);
	}
	protected:
	ImageSource *source;
};


#endif

