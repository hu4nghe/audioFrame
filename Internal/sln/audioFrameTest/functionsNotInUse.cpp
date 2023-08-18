/*
bool resample(size_t targetSampleRate)
    {
        SRC_STATE *srcState = src_new(SRC_SINC_BEST_QUALITY, this->channelNum, nullptr);

        const auto resempleRatio = static_cast<double>(targetSampleRate) / static_cast<double>(this->sampleRate);
        const auto newSize = static_cast<int>(audioData.size() * resempleRatio)+1;
        std::vector<T> resempledAudio(newSize);

        SRC_DATA srcData;
        srcData.data_in = audioData.data();
        srcData.data_out = resempledAudio.data();
        srcData.input_frames = static_cast<long>(audioData.size());
        srcData.output_frames = static_cast<long>(resempledAudio.size());
        srcData.src_ratio = resempleRatio;

        src_process(srcState, &srcData);

        sampleRate = targetSampleRate;
        return true;
    }*/