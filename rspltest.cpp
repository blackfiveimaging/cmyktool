#include <iostream>

#include "debug.h"

extern "C" {
#include "rspl.h"
}

using namespace std;

#include "configdb.h"
#include "imagesource_util.h"
#include "util.h"
#include "tiffsave.h"
#include "progresstext.h"
#include "profilemanager.h"

class RSPLWrapper
{
	public:
	RSPLWrapper(int ind, int outd) : interp(NULL), ind(ind), outd(outd)
	{
		interp=new_rspl(0,ind,outd); // Create a new RSPL structure
		for(int i=0;i<ind;++i)
		{
			gres[i]=16;
			avgdev[i]=0.1;
		}
	}
	virtual ~RSPLWrapper()
	{
		if(interp)
			interp->del(interp);
	}
	virtual bool Populate(int count, double *in, double *out,double smoothing)
	{
		cow *myco=new cow[count];
		datai ilow,ihigh;
		datao olow,ohigh;

		for(int i=0;i<ind;++i)
		{
			ilow[i]=ihigh[i]=in[i];
		}
		for(int i=0;i<outd;++i)
		{
			olow[i]=ohigh[i]=out[i];
		}

		for(int i=0;i<count;++i)
		{
			for(int j=0;j<ind;++j)
			{
				myco[i].p[j]=in[i*ind+j];
				if(in[i*ind+j]<ilow[j]) ilow[j]=in[i*ind+j];
				if(in[i*ind+j]>ihigh[j]) ihigh[j]=in[i*ind+j];
			}
			for(int j=0;j<outd;++j)
			{
				myco[i].v[j]=out[i*outd+j];
				if(out[i*outd+j]<olow[j]) olow[j]=out[i*outd+j];
				if(out[i*outd+j]>ohigh[j]) ohigh[j]=out[i*outd+j];
			}
			myco[i].w=1.0;
		}
		// Increase weights of endpoints...
		myco[0].w=10.0;
		myco[count-1].w=10.0;

		// Add some "headroom"
		for(int j=0;j<ind;++j)
		{
			ilow[j]-=1.0;
			ihigh[j]+=1.0;
		}

		for(int j=0;j<outd;++j)
		{
			olow[j]-=1.0;
			ohigh[j]+=1.0;
		}

		interp->fit_rspl_w(interp,
		           RSPL_EXTRAFIT,				/* Non-mon and clip flags */
		           myco,			/* Test points */
		           count,				/* Number of test points */
		           ilow, ihigh, gres,		/* Low, high, resolution of grid */
		           olow, ohigh,			/* Default data scale */
		           smoothing,			/* Smoothing */
		           avgdev,				/* Average deviation */
		           NULL);				/* iwidth */
		delete[] myco;
		return(true);
	}

	virtual void Interp(double *in, double *out)
	{
		co point;
		for(int i=0;i<ind;++i)
			point.p[i]=*in++;
		for(int i=0;i<outd;++i)
			point.v[i]=0.0;
		interp->interp(interp,&point);
		for(int i=0;i<outd;++i)
			*out++=point.v[i];
	}

	virtual bool ReverseInterp(double *out, double *in)
	{
		co point;
		for(int i=0;i<ind;++i)
			point.p[i]=0;
		for(int i=0;i<outd;++i)
			point.v[i]=*out++;
		int result=interp->rev_interp(interp,RSPL_WILLCLIP|RSPL_NEARCLIP,1,NULL,NULL,&point);
		for(int i=0;i<outd;++i)
			*in++=point.p[i];
		if(result==0)
		{
			Debug[TRACE] << "ReverseInterp: no solutions found" << endl;
			return(false);
		}
		return(true);
	}
	protected:
	rspl *interp;
	int ind,outd;
	int gres[MXDI];
	double avgdev[MXDO];
};


class LabToCMY : public RSPLWrapper
{
	public:
	LabToCMY(CMSProfile *cmykprofile) : RSPLWrapper(3,3)
	{
		CMSWhitePoint wp(5000);
		CMSProfile lab(wp);

		CMSTransform trans(cmykprofile,&lab);

		// Create a 6x6x6 grid.
		int dim=6;
		int count=dim*dim*dim;
		double cmyin[count*3];
		double labout[count*3];
		for(int c=0;c<dim;++c)
		{
			for(int m=0;m<dim;++m)
			{
				for(int y=0;y<dim;++y)
				{
					ISDataType cmyk[4];
					cmyk[0]=(c*IS_SAMPLEMAX)/(dim-1);
					cmyk[1]=(m*IS_SAMPLEMAX)/(dim-1);
					cmyk[2]=(y*IS_SAMPLEMAX)/(dim-1);
					cmyk[3]=0;
					ISDataType lab[3];
					trans.Transform(cmyk,lab,1);
					int idx=((c*dim+m)*dim+y)*3;
					cmyin[idx]=double(cmyk[0])/double(IS_SAMPLEMAX);
					cmyin[idx+1]=double(cmyk[1])/IS_SAMPLEMAX;
					cmyin[idx+2]=double(cmyk[2])/IS_SAMPLEMAX;
					labout[idx]=double(lab[0])/IS_SAMPLEMAX;
					labout[idx+1]=double(lab[1])/IS_SAMPLEMAX-0.5;
					labout[idx+2]=double(lab[2])/IS_SAMPLEMAX-0.5;
					Debug[TRACE] << cmyin[idx] << ", " << cmyin[idx+1] << ", " << cmyin[idx+2] << " -> " <<
						labout[idx] << ", " << labout[idx+1] << ", " << labout[idx+2] << endl;
				}
			}
		}
		Populate(count,cmyin,labout,0.05);
	}
	virtual ~LabToCMY()
	{
	}
};


class LabToKR : public RSPLWrapper
{
	public:
	LabToKR(double *colorant_cmyk, CMSProfile *cmykprofile) : RSPLWrapper(2,3)
	{
		CMSWhitePoint wp(5000);
		CMSProfile lab(wp);

		CMSTransform trans(cmykprofile,&lab);

		int dim=15;
		int count=dim*dim;
		double rkin[count*2];
		double labout[count*3];
		for(int r=0;r<dim;++r)
		{
			for(int k=0;k<dim;++k)
			{
				ISDataType cmyk[4];
				cmyk[0]=(colorant_cmyk[0]*r*IS_SAMPLEMAX)/(dim-1);
				cmyk[1]=(colorant_cmyk[1]*r*IS_SAMPLEMAX)/(dim-1);
				cmyk[2]=(colorant_cmyk[2]*r*IS_SAMPLEMAX)/(dim-1);
				cmyk[3]=(k*IS_SAMPLEMAX)/(dim-1);
				ISDataType lab[3];
				trans.Transform(cmyk,lab,1);
				int idxin=(r*dim+k)*2;
				int idxout=(r*dim+k)*3;
				rkin[idxin]=double(cmyk[0])/double(IS_SAMPLEMAX);
				rkin[idxin+1]=double(cmyk[1])/IS_SAMPLEMAX;
				labout[idxout]=double(lab[0])/IS_SAMPLEMAX;
				labout[idxout+1]=double(lab[1])/IS_SAMPLEMAX-0.5;
				labout[idxout+2]=double(lab[2])/IS_SAMPLEMAX-0.5;
				Debug[TRACE] << rkin[idxin] << ", " << rkin[idxin+1] << " -> " <<
					labout[idxout] << ", " << labout[idxout+1] << ", " << labout[idxout+2] << endl;
			}
		}
		Populate(count,rkin,labout,0.05);
	}
	virtual ~LabToKR()
	{
	}
	protected:
};


class RGBToKR : public RSPLWrapper
{
	public:
	RGBToKR(double *colorant_cmyk, CMSProfile *cmykprofile) : RSPLWrapper(3,3)
	{
		CMSProfile rgb;

		CMSTransform trans(cmykprofile,&rgb);

		int dim=15;
		int count=dim*dim;
		double rkin[count*3];
		double labout[count*3];
		for(int r=0;r<dim;++r)
		{
			for(int k=0;k<dim;++k)
			{
				ISDataType cmyk[4];
				cmyk[0]=(colorant_cmyk[0]*r*IS_SAMPLEMAX)/(dim-1);
				cmyk[1]=(colorant_cmyk[1]*r*IS_SAMPLEMAX)/(dim-1);
				cmyk[2]=(colorant_cmyk[2]*r*IS_SAMPLEMAX)/(dim-1);
				cmyk[3]=(k*IS_SAMPLEMAX)/(dim-1);
				ISDataType rgb[3];
				trans.Transform(cmyk,rgb,1);
				int idxin=(r*dim+k)*3;
				int idxout=(r*dim+k)*3;
				rkin[idxin]=double(r)/double(dim-1);
				rkin[idxin+1]=double(k)/double(dim-1);
				rkin[idxin+2]=0;
				labout[idxout]=double(rgb[0])/IS_SAMPLEMAX;
				labout[idxout+1]=double(rgb[1])/IS_SAMPLEMAX-0.5;
				labout[idxout+2]=double(rgb[2])/IS_SAMPLEMAX-0.5;
				Debug[TRACE] << rkin[idxin] << ", " << rkin[idxin+1] << " -> " <<
					labout[idxout] << ", " << labout[idxout+1] << ", " << labout[idxout+2] << endl;
			}
		}
		Populate(count,rkin,labout,0.05);
	}
	virtual ~RGBToKR()
	{
	}
	protected:
};


class ImageSource_DuoTone : public ImageSource
{
	public:
	ImageSource_DuoTone(ImageSource *src,RGBToKR &rgbkr) : ImageSource(src),source(src),rgbkr(rgbkr)
	{
		switch(source->type)
		{
			case IS_TYPE_RGB:
				break;
			default:
				throw "ImageSource_DuoTone - unsupported image type";
				break;
		}
		type=IS_TYPE_CMYK;
		samplesperpixel=4;
		MakeRowBuffer();
		currentrow=-1;
	}
	~ImageSource_DuoTone()
	{
	}
	ISDataType *GetRow(int row)
	{
		if(row==currentrow)
			return(rowbuffer);

		ISDataType *src=source->GetRow(row);

		for(int x=0;x<width;++x)
		{
			double rgb[3];
			rgb[0]=double(src[x*source->samplesperpixel])/IS_SAMPLEMAX;
			rgb[1]=double(src[x*source->samplesperpixel+1])/IS_SAMPLEMAX;
			rgb[2]=double(src[x*source->samplesperpixel+2])/IS_SAMPLEMAX;
			double kr[3];
			rgbkr.ReverseInterp(rgb,kr);

//			Debug[TRACE] << kr[0] << ", " << kr[1] << endl;

			if(kr[0]<0.0)
				kr[0]=0.0;
			if(kr[1]<0.0)
				kr[1]=0.0;
			if(kr[0]>1.0)
				kr[0]=1.0;
			if(kr[1]>1.0)
				kr[1]=1.0;

//			Debug[TRACE] << kr[0] << ", " << kr[1] << endl;

			rowbuffer[x*samplesperpixel]=ISDataType(IS_SAMPLEMAX*kr[0]);
			rowbuffer[x*samplesperpixel+1]=0;
			rowbuffer[x*samplesperpixel+2]=0;
			rowbuffer[x*samplesperpixel+3]=ISDataType(IS_SAMPLEMAX*kr[1]);
		}
		return(rowbuffer);
	}
	protected:
	ImageSource *source;
	RGBToKR &rgbkr;
};


int main(int argc,char **argv)
{
	Debug.SetLevel(TRACE);
	try
	{
		ConfigFile dummyfile;
		ProfileManager profman(&dummyfile,"[Colour Management]");
		profman.SetInt("DefaultCMYKProfileActive",1);
		profman.SetString("DefaultCMYKProfile","ISOcoated_v2_eci.icc");

		CMSProfile *cmyk=profman.GetDefaultProfile(IS_TYPE_CMYK);
		if(!cmyk)
			throw "Can't open default CMYK profile";

		// We create an RSPL for converting between CMY0 (i.e. all possible CMYK colours that
		// don't involve black) and L*ab.  We will use this in reverse to find a CMY value for a duotone
		// secondary colorant R, so we can simulate mixing K and R.
		// We then need a second RSPL which maps between KR and LAB, so having picked an input shade for R
		// in LAB, we pass it through the first RSPL to get a CMY0 value for it, then build a second
		// RSPL combining Ax(CMY0) / BxK vs Lab.

		LabToCMY labcmy(cmyk);


		double red[]={.55,.3,.25};
//		double red[]={.52,.35,.26};
		double redcmy[4];
		labcmy.ReverseInterp(red,redcmy);
		cerr << "C: "<< redcmy[0] << ", M: " << redcmy[1] << ", Y: " <<redcmy[2] << endl;

		RGBToKR rgbkr(redcmy,cmyk);

		if(argc==2)
		{
			ImageSource *is=ISLoadImage(argv[1]);
			char *outfn=BuildFilename(argv[1],"_duo","tif");
			is=new ImageSource_DuoTone(is,rgbkr);
			TIFFSaver ts(outfn,is);
			ProgressText p;
			ts.SetProgress(&p);
			ts.Save();
			delete is;
			free(outfn);
		}
		else
		{
			for(int r=0;r<256;r+=16)
			{
				for(int g=0;g<256;g+=16)
				{
					for(int b=0;b<256;b+=16)
					{
						double rgb[3];
						rgb[0]=double(r)/255.0;
						rgb[1]=double(g)/255.0;
						rgb[2]=double(b)/255.0;
						double kr[3];
						rgbkr.ReverseInterp(rgb,kr);
						Debug[TRACE] << r << ", " << g << ", " << b << " -> " << kr[0] << ", " << kr[1] << endl;
					}
				}
			}
		}

		if(cmyk)
			delete cmyk;
	}

	catch(const char *err)
	{
		Debug[ERROR] << "Error: " << err << endl;
	}
	return(0);
}

