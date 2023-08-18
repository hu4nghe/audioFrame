/*
   bool resample(size_t targetSampleRate)
   {
       SRC_STATE *srcState = src_new(SRC_SINC_BEST_QUALITY, channelNum, nullptr);

       const auto resempleRatio = static_cast<double>(targetSampleRate) / static_cast<double>(this->sampleRate);
       const auto newSize = static_cast<int>(audioData.size() * resempleRatio)+1;

       auto out = new float[audioData.size()];

       float* a = &audioData[0];
       SRC_DATA srcData;
       srcData.data_in = a;
       srcData.data_out = out;
       srcData.src_ratio = resempleRatio;

       std::cout << src_strerror(src_process(srcState, &srcData)) << std::endl;
       std::vector<float> temp(out,out + audioData.size());
       audioData = temp;

       for (auto& i : audioData)
           std::cout << i << std::endl;

       src_delete(srcState);

       sampleRate = targetSampleRate;
       return true;
   }
   */