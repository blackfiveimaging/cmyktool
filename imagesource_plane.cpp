#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "imagesource_plane.h"

using namespace std;

ImageSource_Plane::~ImageSource_Plane()
{
	if(prev)
		delete prev;

	if(source)
		delete source;
}


ISDataType *ImageSource_Plane::GetRow(int row)
{
	int i;

	float cc,cm,cy,ck;
	float sc,sm,sy,sk;
	float c,m,y,k;
	ISDataType *rb;
	ISDataType *dst;
	ISDataType *src=source->GetRow(row);

	if(prev)
	{
		dst=rb=prev->GetRow(row);
	}
	else
	{
		dst=rb=rowbuffer;
		for(i=0;i<width;++i)
		{
		  dst[i*4]=0.0;
		  dst[i*4+1]=0.0;
		  dst[i*4+2]=0.0;
		  dst[i*4+3]=0.0;
		}
	}

	cc=Colour.c; cc/=255.0;
	cm=Colour.m; cm/=255.0;
	cy=Colour.y; cy/=255.0;
	ck=Colour.k; ck/=255.0;

	switch(source->type)
	{
		case IS_TYPE_GREY:
			for(i=0;i<width;++i)
			{
				sc=*src++;

				c=sc*cc;
				m=sc*cm;
				y=sc*cy;
				k=sc*ck;

				(*dst++)+=c;
				(*dst++)+=m;
				(*dst++)+=y;
				(*dst++)+=k;
			}
			break;
		case IS_TYPE_CMYK:
			for(i=0;i<width;++i)
			{
				sc=*src++;
				sm=*src++;
				sy=*src++;
				sk=*src++;

				c=sc*cc;
				m=sm*cm;
				y=sy*cy;
				k=sk*ck;

				(*dst++)+=c;
				(*dst++)+=m;
				(*dst++)+=y;
				(*dst++)+=k;
			}
			break;
		default:
			break;
	}
	return(rb);
}


ImageSource_Plane::ImageSource_Plane(struct ImageSource *source,struct CMYKValue *colour,struct ImageSource *prev) :
	ImageSource(source), source(source), Colour(*colour), prev(prev)
{
	type=IS_TYPE_CMYK;
	samplesperpixel=4;

	MakeRowBuffer();
}
