/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-05
 */
#ifndef AUSH_MULTIPLEX_H_INCLUDED_20110805
#define AUSH_MULTIPLEX_H_INCLUDED_20110805

namespace aush{

    inline void demultiplex(float *dst, unsigned char *src,
                            unsigned int ch,
                            std::size_t numSamples,
                            unsigned int numBlockAlign,
                            unsigned int bytesPerSample)
    {
        src += ch * bytesPerSample;

        if(bytesPerSample == 1){
            for(std::size_t i = 0; i < numSamples; ++i){
                *dst = (*src - 128) / 128.f;
                ++dst;
                src += numBlockAlign;
            }
        }
        else if(bytesPerSample == 2){
            for(std::size_t i = 0; i < numSamples; ++i){
                const int udata = src[0] | (src[1]<<8);
                const int data = udata < 32768 ? udata : udata - 0x10000;
                *dst = data / 32768.f;
                ++dst;
                src += numBlockAlign;
            }
        }
    }

    inline void multiplex(unsigned char *dst, float *src,
                          unsigned int ch,
                          std::size_t numSamples,
                          unsigned int numBlockAlign,
                          unsigned int bytesPerSample)
    {
        dst += ch * bytesPerSample;

        if(bytesPerSample == 1){
            for(std::size_t i = 0; i < numSamples; ++i){
                int data = static_cast<int>(*src * 128.f + 128);
                if(data < 0){
                    data = 0;
                }
                else if(data > 255){
                    data = 255;
                }
                *dst = static_cast<unsigned char>(data);
                ++src;
                dst += numBlockAlign;
            }
        }
        else if(bytesPerSample == 2){
            for(std::size_t i = 0; i < numSamples; ++i){
                int data = static_cast<int>(*src * 32768.f);
                if(data < -32768){
                    data = -32768;
                }
                else if(data > 32767){
                    data = 32767;
                }
                dst[0] = data & 0xff;
                dst[1] = (data>>8) & 0xff;
                ++src;
                dst += numBlockAlign;
            }
        }
    }

}//namespace aush
#endif
