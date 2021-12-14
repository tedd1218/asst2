#ifndef CAMERA_SENSOR_H
#define CAMERA_SENSOR_H

class CameraSensor
{
public:
	virtual ~CameraSensor() {}
	virtual int GetImageWidth() = 0;
	virtual int GetImageHeight() = 0;
	virtual void SetFocus(int focalPlane) = 0; // From 450 to 500,000 (unit: mm)
	virtual void ReadSensorData(unsigned char * buffer, int left, int top, int width, int height) = 0;
};

CameraSensor * CreateSensor(const wchar_t * sceneName, int noiseLevel, bool badSensor);

#endif