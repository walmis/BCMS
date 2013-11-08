/*
 * terminal.hpp
 *
 *  Created on: Nov 8, 2013
 *      Author: walmis
 */

#ifndef TERMINAL_HPP_
#define TERMINAL_HPP_

#include <xpcc/architecture.hpp>

class Terminal : xpcc::TickerTask {

public:
	Terminal(xpcc::IODevice &device) : device(device) {}


protected:
	virtual void handleCommand(uint8_t nargs, char* argv[]) = 0;

	void handleTick() override {

		if(device.read(buffer[pos])) {
			if(buffer[pos] == '\n') {
				//remove the newline character
				buffer[pos] = 0;
				parse();
				pos = 0;
				return;
			}
			pos++;
			pos &= 31;
		}
	}

	char buffer[32];
	uint8_t pos = 0;

	xpcc::IODevice& device;

	void parse() {
		XPCC_LOG_DEBUG .printf(": %s\n", buffer);

		char* tok = strtok(buffer, " ");

		char* arglist[6];
		uint8_t nargs = 0;

		memset(arglist, 0, sizeof(char*)*6);

		while(tok && nargs < 5) {
			arglist[nargs] = tok;
			tok = strtok(0, " ");
			nargs++;
		}

		handleCommand(nargs, arglist);

	}

	int to_int(char *p) {
		int k = 0;
		bool neg = false;
		if(*p == '-') {
			p++;
			neg = true;
		}

		while (*p) {
			k = (k << 3) + (k << 1) + (*p) - '0';
			p++;
		}
		if(neg) return -k;
		return k;
	}

	bool float_scan(const wchar_t* wcs, float* val)
	// (C)2009 Marcin Sokalski gumix@ghnet.pl - All rights reserved.
			{
		int hdr = 0;
		while (wcs[hdr] == L' ')
			hdr++;

		int cur = hdr;

		bool negative = false;
		bool has_sign = false;

		if (wcs[cur] == L'+' || wcs[cur] == L'-') {
			if (wcs[cur] == L'-')
				negative = true;
			has_sign = true;
			cur++;
		} else
			has_sign = false;

		int quot_digs = 0;
		int frac_digs = 0;

		bool full = false;

		wchar_t period = 0;
		int binexp = 0;
		int decexp = 0;
		unsigned long value = 0;

		while (wcs[cur] >= L'0' && wcs[cur] <= L'9') {
			if (!full) {
				if (value >= 0x19999999 && wcs[cur] - L'0' > 5
						|| value > 0x19999999) {
					full = true;
					decexp++;
				} else
					value = value * 10 + wcs[cur] - L'0';
			} else
				decexp++;

			quot_digs++;
			cur++;
		}

		if (wcs[cur] == L'.' || wcs[cur] == L',') {
			period = wcs[cur];
			cur++;

			while (wcs[cur] >= L'0' && wcs[cur] <= L'9') {
				if (!full) {
					if (value >= 0x19999999 && wcs[cur] - L'0' > 5
							|| value > 0x19999999)
						full = true;
					else {
						decexp--;
						value = value * 10 + wcs[cur] - L'0';
					}
				}

				frac_digs++;
				cur++;
			}
		}

		if (!quot_digs && !frac_digs)
			return false;

		wchar_t exp_char = 0;

		int decexp2 = 0; // explicit exponent
		bool exp_negative = false;
		bool has_expsign = false;
		int exp_digs = 0;

		// even if value is 0, we still need to eat exponent chars
		if (wcs[cur] == L'e' || wcs[cur] == L'E') {
			exp_char = wcs[cur];
			cur++;

			if (wcs[cur] == L'+' || wcs[cur] == L'-') {
				has_expsign = true;
				if (wcs[cur] == '-')
					exp_negative = true;
				cur++;
			}

			while (wcs[cur] >= L'0' && wcs[cur] <= L'9') {
				if (decexp2 >= 0x19999999)
					return false;
				decexp2 = 10 * decexp2 + wcs[cur] - L'0';
				exp_digs++;
				cur++;
			}

			if (exp_negative)
				decexp -= decexp2;
			else
				decexp += decexp2;
		}

		// end of wcs scan, cur contains value's tail

		if (value) {
			while (value <= 0x19999999) {
				decexp--;
				value = value * 10;
			}

			if (decexp) {
				// ensure 1bit space for mul by something lower than 2.0
				if (value & 0x80000000) {
					value >>= 1;
					binexp++;
				}

				if (decexp > 308 || decexp < -307)
					return false;

				// convert exp from 10 to 2 (using FPU)
				int E;
				double v = pow(10.0, decexp);
				double m = frexp(v, &E);
				m = 2.0 * m;
				E--;
				value = (unsigned long) floor(value * m);

				binexp += E;
			}

			binexp += 23; // rebase exponent to 23bits of mantisa

			// so the value is: +/- VALUE * pow(2,BINEXP);
			// (normalize manthisa to 24bits, update exponent)
			while (value & 0xFE000000) {
				value >>= 1;
				binexp++;
			}
			if (value & 0x01000000) {
				if (value & 1)
					value++;
				value >>= 1;
				binexp++;
				if (value & 0x01000000) {
					value >>= 1;
					binexp++;
				}
			}

			while (!(value & 0x00800000)) {
				value <<= 1;
				binexp--;
			}

			if (binexp < -127) {
				// underflow
				value = 0;
				binexp = -127;
			} else if (binexp > 128)
				return false;

			//exclude "implicit 1"
			value &= 0x007FFFFF;

			// encode exponent
			unsigned long exponent = (binexp + 127) << 23;
			value |= exponent;
		}

		// encode sign
		unsigned long sign = negative << 31;
		value |= sign;

		if (val) {
			*(unsigned long*) val = value;
		}

		return true;
	}
};


#endif /* TERMINAL_HPP_ */
