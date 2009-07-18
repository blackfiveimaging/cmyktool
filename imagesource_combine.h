#ifndef IMAGESOURCE_COMBINE_H
#define IMAGESOURCE_COMBINE_H

#include "imagesource.h"

class ImageSource_Combine : public ImageSource
{
	public:
	ImageSource_Combine(ImageSource *source,ISDeviceNValue &colour,ImageSource *prev,IS_TYPE type=IS_TYPE_CMYK,int channels=4);
	~ImageSource_Combine();
	ISDataType *GetRow(int row);
	private:
	ImageSource *source;
	ISDeviceNValue colour;
	ImageSource *prev;
};

#endif
