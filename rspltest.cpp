#include <iostream>

#include "debug.h"

extern "C" {
#include "rspl.h"
}

using namespace std;

#include "configdb.h"
#include "profilemanager.h"


class LabToCMY
{
	public:
	LabToCMY() : interp(NULL)
	{
		ind=outd=3;
		interp=new_rspl(0,ind,outd); // Create a new RSPL structure
		for(int i=0;i<ind;++i)
		{
			gres[i]=16;
			avgdev[i]=0.1;
		}
	}
	~LabToCMY()
	{
		if(interp)
			interp->del(interp);
	}
	bool Populate(int count, double *in, double *out,double smoothing)
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

	void Interp(double *in, double *out)
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

	bool ReverseInterp(double *out, double *in)
	{
		co point;
		for(int i=0;i<ind;++i)
			point.v[i]=0;
		for(int i=0;i<outd;++i)
			point.p[i]=*out++;
		int result=interp->rev_interp(interp,0,1,NULL,NULL,&point);
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
	int ind,outd;
	rspl *interp;
	int gres[MXDI];
	double avgdev[MXDO];
	friend class Interpolate_1D;
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

		CMSWhitePoint wp(5000);
		CMSProfile lab(wp);
		CMSProfile *cmyk=profman.GetDefaultProfile(IS_TYPE_CMYK);
		if(!cmyk)
			throw "Can't open default CMYK profile";

		CMSTransform trans(cmyk,&lab);

		// We create an RSPL for converting between CMY0 (i.e. all possible CMYK colours that
		// don't involve black) and L*ab.  We will use this in reverse to find a CMY value for a duotone
		// secondary colorant R, so we can simulate mixing K and R.
		// We then need a second RSPL which maps between KR and LAB, so having picked an input shade for R
		// in LAB, we pass it through the first RSPL to get a CMY0 value for it, then build a second
		// RSPL combining Ax(CMY0) / BxK vs Lab.

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
					Debug[TRACE] << cmyk[0] << ", " << cmyk[1] << ", " << cmyk[2] << " -> " <<
						lab[0]*100/IS_SAMPLEMAX << ", " << lab[1]*100/IS_SAMPLEMAX-50 << ", " << lab[2]*100/IS_SAMPLEMAX-50 << endl;
					cmyin[((c*dim+m)*dim+y)*3]=cmyk[0];
					cmyin[((c*dim+m)*dim+y)*3+1]=cmyk[1];
					cmyin[((c*dim+m)*dim+y)*3+2]=cmyk[2];
					labout[((c*dim+m)*dim+y)*3]=lab[0];
					labout[((c*dim+m)*dim+y)*3+1]=lab[1];
					labout[((c*dim+m)*dim+y)*3+2]=lab[2];
				}
			}
		}
		LabToCMY interp;
		interp.Populate(count,cmyin,labout,0.05);

		if(cmyk)
			delete cmyk;
	}

	catch(const char *err)
	{
		Debug[ERROR] << "Error: " << err << endl;
	}
	return(0);
}

