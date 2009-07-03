/*
 * imagesource_deflatten.cpp
 * ImageSource Colour Management filter.
 *
 * Copyright (c) 2005 by Alastair M. Robinson
 * Distributed under the terms of the GNU General Public License -
 * see the file named "COPYING" for more details.
 *
 * TODO: Clean up handling of image types other than RGB
 */

#include <iostream>
#include <stdlib.h>

#include <math.h>

#include "imagesource_deflatten.h"
#include "imagesource_flatten.h"

using namespace std;

ImageSource_Deflatten::~ImageSource_Deflatten()
{
	free(tmp1);
	free(tmp2);

	if(source)
		delete source;
		
	if(disposetransform)
		delete transform;
}


ISDataType *ImageSource_Deflatten::GetRow(int row)
{
	ISDataType *src;
	static int lc=0,lm=0,ly=0;

	if(row==currentrow)
		return(rowbuffer);

	src=source->GetRow(row);

	// Copy just the colour data from src to tmp1, ignoring alpha
	// (FIXME: separate code-path for the non-alpha case would be quicker)
	for(int i=0;i<width;++i)
	{
		for(int j=0;j<tmpsourcespp;++j)
			tmp1[i*tmpsourcespp+j]=(65535*src[i*source->samplesperpixel+j])/IS_SAMPLEMAX;
	}

	transform->Transform(tmp1,tmp2,width);

	if(preservegrey)
	{
		for(int i=0;i<width;++i)
		{
			int r=*src++;
			int g=*src++;
			int b=*src++;

			if((r==g) && (g==b))
			{
				int grey=IS_SAMPLEMAX-g;
				rowbuffer[i*samplesperpixel]=0;
				rowbuffer[i*samplesperpixel+1]=0;
				rowbuffer[i*samplesperpixel+2]=0;
				rowbuffer[i*samplesperpixel+3]=grey;
			}
			else
			{
				rowbuffer[i*samplesperpixel]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp])/65535;
				rowbuffer[i*samplesperpixel+1]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp+1])/65535;
				rowbuffer[i*samplesperpixel+2]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp+2])/65535;
				rowbuffer[i*samplesperpixel+3]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp+3])/65535;
			}			
		}
	}
	else if(preserveblack)
	{
		for(int i=0;i<width;++i)
		{
			int r=*src++;
			int g=*src++;
			int b=*src++;

			if((r|g|b)!=0)
			{
				int nc,nm,ny;
				nc=rowbuffer[i*samplesperpixel]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp])/65535;
				nm=rowbuffer[i*samplesperpixel+1]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp+1])/65535;
				ny=rowbuffer[i*samplesperpixel+2]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp+2])/65535;
				rowbuffer[i*samplesperpixel+3]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp+3])/65535;
				lc=(5*lc+nc)/6;
				lm=(5*lm+nm)/6;
				ly=(5*ly+ny)/6;
			}
			else if (overprintblack)
			{
				rowbuffer[i*samplesperpixel]=lc;
				rowbuffer[i*samplesperpixel+1]=lm;
				rowbuffer[i*samplesperpixel+2]=ly;
				rowbuffer[i*samplesperpixel+3]=IS_SAMPLEMAX;
			}
			else
			{
				rowbuffer[i*samplesperpixel]=0;
				rowbuffer[i*samplesperpixel+1]=0;
				rowbuffer[i*samplesperpixel+2]=0;
				rowbuffer[i*samplesperpixel+3]=IS_SAMPLEMAX;
			}			
		}
	}
	else
	{
		for(int i=0;i<width;++i)
		{
			// Copy just the colour data from tmp2 to rowbuffer, ignoring alpha.
			for(int j=0;j<tmpdestspp;++j)
				rowbuffer[i*samplesperpixel+j]=(IS_SAMPLEMAX*tmp2[i*tmpdestspp+j])/65535;
		}
	}

	// Copy alpha channel unchanged if present
//	if(HAS_ALPHA(source->type))
//	{
//		for(int i=0;i<width;++i)
//			rowbuffer[(i+1)*samplesperpixel-1]=src[(i+1)*source->samplesperpixel-1];
//	}

	currentrow=row;
	return(rowbuffer);
}


ImageSource_Deflatten::ImageSource_Deflatten(ImageSource *source,CMSProfile *inp,CMSProfile *outp,bool preserveblack,bool overprintblack,bool preservegrey)
	: ImageSource(source), source(source), preserveblack(preserveblack), overprintblack(overprintblack), preservegrey(preservegrey)
{
	transform=new CMSTransform(inp,outp);
	disposetransform=true;

	Init();
}


ImageSource_Deflatten::ImageSource_Deflatten(ImageSource *source,CMSTransform *transform,bool preserveblack,bool overprintblack,bool preservegrey)
	: ImageSource(source), source(source), transform(transform), preserveblack(preserveblack), overprintblack(overprintblack), preservegrey(preservegrey)
{
	disposetransform=false;

	Init();
}


void ImageSource_Deflatten::Init()
{
	cerr << "Initialsing CMS transform" << endl;
	switch(type=transform->GetOutputColourSpace())
	{
		case IS_TYPE_CMYK:
			samplesperpixel=4;
			break;
		default:
			throw "Deflatten only supports CMYK output";
			break;
	}

	cerr << "Type: " << type << endl;

	if(transform->GetInputColourSpace()!=STRIP_ALPHA(source->type))
	{
		throw "Source image must match source profile!";
	}

	if(HAS_ALPHA(source->type))
		source=new ImageSource_Flatten(source);

	tmpsourcespp=source->samplesperpixel;
	tmpdestspp=samplesperpixel;
	if(HAS_ALPHA(source->type))
	{
		++samplesperpixel;
		--tmpsourcespp;
		type=IS_TYPE(type | IS_TYPE_ALPHA);
	}

	tmp1=(unsigned short *)malloc(sizeof(unsigned short)*width*source->samplesperpixel);
	tmp2=(unsigned short *)malloc(sizeof(unsigned short)*width*samplesperpixel);

	cerr << "tmpsourcespp: " << tmpsourcespp << endl;
	cerr << "tmpdestspp: " << tmpdestspp << endl;
	cerr << "samplesperpixel: " << samplesperpixel << endl;

	MakeRowBuffer();
}
