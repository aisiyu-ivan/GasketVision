#include "IVisionImageSource.h"

#include <opencv2/core.hpp>

#include <cstring>

bool IVisionImageSource::grabFrameInto(uchar *dst, int capacity, VisionFrame &out)
{
    if (!dst || capacity <= 0)
        return false;
    if (!grabFrame(out))
        return false;

    cv::Mat continuous = out.image.isContinuous() ? out.image : out.image.clone();
    const int nbytes = static_cast<int>(static_cast<qsizetype>(continuous.step) * continuous.rows);
    if (nbytes <= 0 || nbytes > capacity)
        return false;

    std::memcpy(dst, continuous.data, static_cast<size_t>(nbytes));
    out.image =
        cv::Mat(continuous.rows, continuous.cols, continuous.type(), dst, static_cast<size_t>(continuous.step));
    return true;
}
