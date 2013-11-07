/*
 * operation.cpp
 *
 *  Created on: Oct 30, 2013
 *      Author: walmis
 */

#include "SDCardAsync.h"

Operation* Operation::base = 0;

	Operation::Operation() {
		next = 0;
		if (base == 0) {
			base = this;
		} else {
			Operation* t = base;
			while (t) {
				if (t->next == 0) {
					t->next = this;
					break;
				}
				t = t->next;
			}
		}
		reset();
	}

	Operation::~Operation() {
		if (base == this) {
			base = next;
		} else {
			Operation* t = base;
			while (t) {
				if (t->next == this) {
					t->next = next;
					break;
				}
				t = t->next;
			}
		}
	}



