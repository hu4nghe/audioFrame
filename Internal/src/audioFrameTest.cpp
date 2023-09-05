#include "audioFrame.h"

int main()
{
    audioQueue<float> data(10);
    data.setChannelNum(2);
    data.setSampleRate(44100);

    float a[5] = { 1.2, 3.6, 4.8, 9.5, 56.871 };
    data.push(a, 5);

    float* out = new float[5];
    data.pop(out, 11);

    for (int i = 0; i < 5; i++)
        std::cout << out[i] << std::endl;
    return 0;

}