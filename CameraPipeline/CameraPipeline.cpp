#include "CameraSensor.h"
#include "Image.h"
#include <algorithm>
#include <cmath>
#include <vector>

class CameraPipeline
{
private:
    CameraSensor* sensor;
    int initialFocus;

private:
    List<bool> defectMap;
    List<float> vignetteMap;
    bool isCalibrated;

    enum BayerColor { RED, GREEN, BLUE };

    BayerColor GetBayerColor(int x, int y)
    {
        if (y % 2 == 0 && x % 2 == 0) return GREEN;
        if (y % 2 == 0 && x % 2 == 1) return RED;
        if (y % 2 == 1 && x % 2 == 0) return BLUE;
        return GREEN;
    }

    void CalibrateCamera(int w, int h)
    {
        if (isCalibrated) return;

        List<unsigned char> dark;
        dark.SetSize(w * h);
        sensor->ReadSensorData(dark.Buffer(), 0, 0, w, h);

        defectMap.SetSize(w * h);
        vignetteMap.SetSize(w * h);
        
        float cx = w / 2.0f;
        float cy = h / 2.0f;
        float maxDistanceSquared = cx*cx + cy*cy;
        
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                int i = y * w + x;
                defectMap[i] = (dark[i] < 10 || dark[i] > 245);
                float dx = x - cx;
                float dy = y - cy;
                float distanceSquaredNorm = (dx*dx + dy*dy) / maxDistanceSquared;
                vignetteMap[i] = 1.0f + distanceSquaredNorm * 0.35f;
            }
        }
        
        isCalibrated = true;
    }

    unsigned char GetPixel(unsigned char* buf, int x, int y, int w, int h)
    {
        if (x < 0 || x >= w || y < 0 || y >= h) return 0;
        
        int idx = y * w + x;
        if (!defectMap[idx]) return buf[idx];
        
        BayerColor color = GetBayerColor(x, y);
        int sum = 0, count = 0;
        
        for (int dy = -2; dy <= 2; dy++)
        {
            for (int dx = -2; dx <= 2; dx++)
            {
                if (dx == 0 && dy == 0) continue;
                
                int nx = x + dx;
                int ny = y + dy;
                
                if (nx >= 0 && nx < w && ny >= 0 && ny < h)
                {
                    int nidx = ny * w + nx;
                    if (GetBayerColor(nx, ny) == color && !defectMap[nidx])
                    {
                        sum += buf[nidx];
                        count++;
                    }
                }
            }
        }
        
        return count > 0 ? sum / count : buf[idx];
    }

    unsigned char Interpolate(unsigned char* buf, int x, int y, int w, int h, BayerColor target)
    {
        BayerColor currentBayerColor = GetBayerColor(x, y);
        if (currentBayerColor == target) return GetPixel(buf, x, y, w, h);
        int sum = 0, count = 0;
        int dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
        int dy[] = {0, 0, -1, 1, -1, -1, 1, 1};
        bool useDiagonal = (currentBayerColor != GREEN && target != GREEN);
        int start = useDiagonal ? 4 : 0;
        int end = useDiagonal ? 8 : 4;
        
        for (int i = start; i < end; i++)
        {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            if (nx >= 0 && nx < w && ny >= 0 && ny < h)
            {
                bool valid = (currentBayerColor == GREEN) ? (GetBayerColor(nx, ny) == target) : true;
                if (valid)
                {
                    sum += GetPixel(buf, nx, ny, w, h);
                    count++;
                }
            }
        }
        
        return count > 0 ? sum / count : 0;
    }

    void CorrectStripes(unsigned char* buf, int w, int h)
    {
        List<float> rowAvg;
        rowAvg.SetSize(h);
        float globalSum = 0.0f;

        for (int y = 0; y < h; y++)
        {
            float sum = 0.0f;
            for (int x = 0; x < w; x++)
                sum += buf[y*w + x];
            rowAvg[y] = sum / w;
            globalSum += rowAvg[y];
        }

        float globalAvg = globalSum / h;

        for (int y = 0; y < h; y++)
        {
            float offset = rowAvg[y] - globalAvg;
            for (int x = 0; x < w; x++)
            {
                int idx = y * w + x;
                buf[idx] = Math::Clamp((int)(buf[idx] - offset), 0, 255);
            }
        }
    }


    void AutoFocus()
    {
        // sensor->SetFocus(initialFocus);
        const int focusMin = 450, focusMax = 500000;
        int width = sensor->GetImageWidth(), height = sensor->GetImageHeight();

        auto FocusScore = [&](int focusVal) -> double {
            sensor->SetFocus(focusVal);

            int cropW = Math::Clamp(width / 8, 32, 128);
            int cropH = Math::Clamp(height / 8, 32, 128);
            int cropX = (width - cropW) / 2;
            int cropY = (height - cropH) / 2;

            List<unsigned char> patch;
            patch.SetSize(cropW * cropH);
            sensor->ReadSensorData(patch.Buffer(), cropX, cropY, cropW, cropH);

            double total = 0; int count = 0;
            for (int y = 1; y < cropH - 1; y++)
                for (int x = 1; x < cropW - 1; x++)
                    if (GetBayerColor(cropX + x, cropY + y) == GREEN) {
                        int center = patch[y*cropW + x];
                        int lap = 4 * center - patch[(y-1)*cropW + x] - patch[(y+1)*cropW + x]
                                            - patch[y*cropW + x-1] - patch[y*cropW + x+1];
                        total += std::abs(lap);
                        count++;
                    }
            return count ? total / count : 0.0;
        };

        int focusNow = Math::Clamp(initialFocus, focusMin, focusMax);
        double bestScore = FocusScore(focusNow);
        int step = 5000;

        double rightScore = FocusScore(focusNow + step);
        double leftScore  = FocusScore(focusNow - step);

        int direction = 0;
        if (rightScore > bestScore) {
            direction = +1;
            bestScore = rightScore;
        } 
        else if (leftScore > bestScore) {
            direction = -1;
            bestScore = leftScore;
        }

        while (direction != 0) {
            int nextFocus = Math::Clamp(focusNow + direction * step, focusMin, focusMax);
            double nextScore = FocusScore(nextFocus);
            if (nextScore > bestScore) {
                focusNow = nextFocus;
                bestScore = nextScore;
                step = std::min(step * 2, 100000);
            } else break;
        }

        int low = std::max(focusMin, focusNow - step);
        int high = std::min(focusMax, focusNow + step);
        for (int i = 0; i < 8 && low < high; i++) {
            int mid1 = low + (high - low) / 3;
            int mid2 = high - (high - low) / 3;
            double score1 = FocusScore(mid1);
            double score2 = FocusScore(mid2);
            if (score1 < score2) { 
                low = mid1; 
                focusNow = mid2; 
                bestScore = score2; 
            }
            else { 
                high = mid2; 
                focusNow = mid1; 
                bestScore = score1; 
            }
        }

        sensor->SetFocus(focusNow);
    }

    void ProcessShot(Image & result, unsigned char * inputBuffer, int w, int h)
	{
        CorrectStripes(inputBuffer, w, h);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++)
            {
                int idx = y * w + x;
                float vig = vignetteMap[idx];
                
                result.Pixels[idx].r = Math::Clamp((int)(Interpolate(inputBuffer, x, y, w, h, RED) * vig + 0.5f), 0, 255);
                result.Pixels[idx].g = Math::Clamp((int)(Interpolate(inputBuffer, x, y, w, h, GREEN) * vig + 0.5f), 0, 255);
                result.Pixels[idx].b = Math::Clamp((int)(Interpolate(inputBuffer, x, y, w, h, BLUE) * vig + 0.5f), 0, 255);
            }
        }
    }

public:

    CameraPipeline(CameraSensor * sensor, int initialFocus)
    {
        this->sensor = sensor;
        this->initialFocus = initialFocus;
        this->isCalibrated = false;
    }

    // Obtain pixel values from sensor, process the sensor output, and
    // then stick RGB values into the pixels of 'result'
    void TakePicture(Image & result)
    {
        result.Width = sensor->GetImageWidth();
        result.Height = sensor->GetImageHeight();
        result.Pixels.SetSize(result.Width * result.Height);

        CalibrateCamera(result.Width, result.Height);

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
