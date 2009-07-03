#ifndef IMAGESOURCE_PLANE_H
#define IMAGESOURCE_PLANE_H

#include "imagesource.h"
#include "cmyktypes.h"

class ImageSource_Plane : public ImageSource
{
	public:
	ImageSource_Plane(ImageSource *source,struct CMYKValue *colour,ImageSource *prev);
	~ImageSource_Plane();
	ISDataType *GetRow(int row);
	private:
	ImageSource *source;
	struct CMYKValue Colour;
	ImageSource *prev;
};

#endif
