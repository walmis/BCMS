/*
 * Filters.h
 *
 *  Created on: Apr 5, 2012
 *      Author: walmis
 */

#ifndef FILTERS_H_
#define FILTERS_H_

template <typename T, int strength>
class IIRFilter {
		bool first;
        T sum;
public:
        IIRFilter(T initial = 0) {
                sum = initial * strength;
                first = false;
        }

        inline T push(T sample) {
        	if(!first) {
        		sum = sample * strength;
        		first = true;
        	}
            sum = sum - (sum / strength) + sample;
            return sum / strength;
        }

        inline T getValue() {
                return sum / strength;
        }
        inline T operator << (T v) {
        	return push(v);
        }
};

template <typename T, int samples>
class Average {
public:
        uint16_t count;
        T avg;

        T push(T val) {
        	avg = avg + ((val-avg) / ++count);
        	if(count > samples) {
        		count = 0;
        	}
            return avg;
        }
        T getValue() {
                return avg;
        }

        void reset() {
                count = 0;
                avg = 0;
        }

};

class FastAverage {
        int32_t sum;
        uint32_t num_samples;
public:
        void push(int32_t val) {
                sum += val;
                num_samples++;
        }
        int32_t get_val() {
                return sum / num_samples;
        }
        void reset() {
                sum = 0;
                num_samples = 0;
        }

};

int median5(int a, int b, int c, int d, int e)
{
    return b < a ? d < c ? b < d ? a < e ? a < d ? e < d ? e : d
                                                 : c < a ? c : a
                                         : e < d ? a < d ? a : d
                                                 : c < e ? c : e
                                 : c < e ? b < c ? a < c ? a : c
                                                 : e < b ? e : b
                                         : b < e ? a < e ? a : e
                                                 : c < b ? c : b
                         : b < c ? a < e ? a < c ? e < c ? e : c
                                                 : d < a ? d : a
                                         : e < c ? a < c ? a : c
                                                 : d < e ? d : e
                                 : d < e ? b < d ? a < d ? a : d
                                                 : e < b ? e : b
                                         : b < e ? a < e ? a : e
                                                 : d < b ? d : b
                 : d < c ? a < d ? b < e ? b < d ? e < d ? e : d
                                                 : c < b ? c : b
                                         : e < d ? b < d ? b : d
                                                 : c < e ? c : e
                                 : c < e ? a < c ? b < c ? b : c
                                                 : e < a ? e : a
                                         : a < e ? b < e ? b : e
                                                 : c < a ? c : a
                         : a < c ? b < e ? b < c ? e < c ? e : c
                                                 : d < b ? d : b
                                         : e < c ? b < c ? b : c
                                                 : d < e ? d : e
                                 : d < e ? a < d ? b < d ? b : d
                                                 : e < a ? e : a
                                         : a < e ? b < e ? b : e
                                                 : d < a ? d : a;
}

int median3(int a, int b, int c) {
                        if ((a < b && c > b) || (c < b && a > b))
                                return b;

                        if ((c < a && b > a) || (b < a && c > a))
                                return a;

                        if ((b < c && a > c) || (a < c && b > c))
                                return c;
}

//LPC adc sometimes produces glitches
class GlitchFilter {

        int16_t samples[5];
        uint8_t num;
        uint16_t max;
public:
        GlitchFilter() {
                num = 0;
        }

        int16_t push(uint16_t val) {
                uint8_t i;
                if(num == 0) {
                        for(i = 0; i < 5; i++) {
                                samples[i] = val;
                        }
                        num = 1;
                }
#if 1
                if (val > 4000)
                        val = median5(samples[0], samples[1], samples[2], samples[3],
                                        samples[4]);

                for(i = 1; i < 5; i++) {
                        samples[i-1] = samples[i];
                }
                samples[4] = val;

                return median5(samples[0], samples[1], samples[2], samples[3], samples[4]);

#else

                for(i = 1; i < 3; i++) {
                        samples[i-1] = samples[i];
                }
                samples[2] = val;

//              if(abs(samples[2] - samples[1]) > max) {
//                      max = abs(samples[2] - samples[1]);
//                      DBG("max: %d\n", max);
//              }

                //if(samples[2] > 3200) {
                //      DBG("a glitch %d %d %d\n", samples[0], samples[1], samples[2]);
                //}

                if (abs(samples[2] - samples[1]) > 3000) {
                        //XPCC_LOG_DEBUG. printf("glitch %d %d %d\n", samples[0], samples[1], samples[2]);
                        samples[2] = (samples[0] + samples[1]) / 2;

                }

                return median3(samples[0], samples[1], samples[2]);

#endif
        }
};

class Filter {

        uint32_t sum;
        uint8_t strength;
public:
        Filter(uint8_t strength, uint32_t initial = 0) {
                sum = initial << strength;
                this->strength = strength;
        }

        uint32_t push(uint32_t sample) {
                sum = sum - (sum >> strength) + sample;
                return sum >> strength;
        }

        uint32_t get_val() {
                return sum >> strength;
        }
};

#endif /* FILTERS_H_ */
