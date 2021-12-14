#include "CameraSensor.h"
#include "Image.h"

class CameraPipeline
{
private:
    CameraSensor* sensor;
    int initialFocus;

private:

    void AutoFocus()
    {
        sensor->SetFocus(initialFocus);
    }

    void ProcessShot(Image & result, unsigned char * inputBuffer, int w, int h)
	{
        // the current implementation just replicates the pixel's
        // measurement into the RGB channels of the output image
        auto pix = inputBuffer;
        for (int i = 0; i < h; i++)
        {
            for (int j = 0; j < w; j++)
            {
                auto & p = result.Pixels[i*w + j];
                p.r = p.b = p.g = pix[i*w + j];
            }
        }
    }

public:

    CameraPipeline(CameraSensor * sensor, int initialFocus)
    {
        this->sensor = sensor;
        this->initialFocus = initialFocus;
    }

    // Obtain pixel values from sensor, process the sensor output, and
    // then stick RGB values into the pixels of 'result'
    void TakePicture(Image & result)
    {
        result.Width = sensor->GetImageWidth();
        result.Height = sensor->GetImageHeight();
        result.Pixels.SetSize(result.Width * result.Height);

        // buffer for storing bits read from sensor
        List<unsigned char> buffer;
        buffer.SetSize(result.Width * result.Height);

        // focus the camera
        AutoFocus();

        // take a shot (arguments to ReadSensorData() define a
        // cropwindow of the sensor: left, top, width, height)
        //
        // In the example below, we grab the whole sensor
        sensor->ReadSensorData(buffer.Buffer(), 0, 0, result.Width, result.Height);

        // process the shot
        ProcessShot(result, buffer.Buffer(), result.Width, result.Height);
    }
};

void PrintUsage(char* programName)
{
    printf("usage: %s [options] inputfile outputfile\n", programName);
    printf("  Options:\n");
    printf("     -focus INT            Set default focus\n");
    printf("     -noiselevel [1-4]     Set amount of sensor measurement noise (default=1)\n");
    printf("     -verybadsensor        Turn on low-quality sensor mode\n");
    printf("     -help                 This message\n");
    printf("\n");
}

int main(int argc, char* argv[])
{

    int noiseLevel = 1;
    bool badSensor = false;
    int initialFocus = 650;

    int optind = 1;
    int i;
    for (i=1; i < argc; i++)
    {
        if (String(argv[i]) == L"-noiselevel" && i + 1 < argc)
        {
            noiseLevel = Math::Clamp(StringToInt(String(argv[i + 1])), 0, 4);
            i++;
        }
        else if (String(argv[i]) == L"-focus")
        {
            initialFocus = StringToInt(String(argv[i + 1]));
            i++;
        }
        else if (String(argv[i]) == L"-verybadsensor")
            badSensor = true;

        else if (String(argv[i]) == L"-help")
        {
			PrintUsage(argv[0]);
            return 0;
        }
        else
            break;

    }
    optind = i;

    if (optind+1 >= argc)
    {
		PrintUsage(argv[0]);
        return 0;
    }

    String inputFilename = argv[optind];
    String outputFilename = argv[optind+1];

    RefPtr<CameraSensor> sensor = CreateSensor(inputFilename.Buffer(), noiseLevel, badSensor);
    {
        CameraPipeline pipe(sensor.Ptr(), initialFocus);
        Image result;
        pipe.TakePicture(result);
        result.Save(outputFilename);
    }

    return 0;
}
