/* Copyright (c) 2011, onitake <onitake@gmail.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "frame.h"
#include <stdlib.h>
#include <string.h>

DaliFramePtr daliframe_new(uint8_t address, uint8_t command) {
	DaliFramePtr frame = malloc(sizeof(struct DaliFrame));
	memset(frame, 0, sizeof(struct DaliFrame));
	frame->address = address;
	frame->command = command;
	return frame;
}

DaliFramePtr daliframe_enew(uint8_t ecommand, uint8_t address, uint8_t command) {
	DaliFramePtr frame = malloc(sizeof(struct DaliFrame));
	memset(frame, 0, sizeof(struct DaliFrame));
	frame->ecommand = ecommand;
	frame->address = address;
	frame->command = command;
	return frame;
}

DaliFramePtr daliframe_clone(DaliFramePtr frame) {
	DaliFramePtr ret = malloc(sizeof(struct DaliFrame));
	memset(ret, 0, sizeof(struct DaliFrame));
	ret->ecommand = frame->ecommand;
	ret->address = frame->address;
	ret->command = frame->command;
	return ret;
}

void daliframe_free(DaliFramePtr frame) {
	if (frame) {
		free(frame);
	}
}

