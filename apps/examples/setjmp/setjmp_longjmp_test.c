/****************************************************************************
 *
 * Copyright 2017 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <tinyara/config.h>

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define MAX_DEPTH 8
int did_return[MAX_DEPTH];

typedef enum {
	DIVE_RETURN,
	DIVE_LONGJMP,
	DIVE_EXCEPTION
} dive_mode_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// This function does a recursive dive MAX_DEPTH times, each time saving local
// state on stack with setjmp call (ensure stck is big enough to hold them).
//
// Depending on the |mode| it either:
//   DIVE_RETURN - just returns (dive all to the bottom, then return
//                 all the way up, one frame at a time).
//   DIVE_LONGJMP - calls down (until it reaches the bottom), then "returns"
//                 via longjmp to its immediate caller, all the way up.
//   DIVE_EXCEPTION - calls down (until it reaches the bottom), then returns
//                 to the top, skipping intermediate stack frames.
static void dive(int depth, dive_mode_t mode, jmp_buf upper)
{
	fprintf(stderr, "diving(%d, %c, %p)\n", depth, "RLE"[mode], (void *)upper);
	jmp_buf env;

	switch (mode) {
	case DIVE_RETURN:
		assert(setjmp(env) == 0);
		if (depth < MAX_DEPTH) {
			dive(depth + 1, mode, env);
		}
		break;
	case DIVE_LONGJMP:
		if ((setjmp(env) == 0) && (depth < MAX_DEPTH)) {
			dive(depth + 1, mode, env);
			assert(!"setjmp/longjmp failed: should not reach this point");
		}
		fprintf(stderr, "ascending(%d, %c, %p)\n", depth, "RLE"[mode], (void *)upper);
		// 25 ms
		usleep(25000);
		did_return[depth - 1] = -1;
		longjmp(upper, 1);
		break;
	case DIVE_EXCEPTION:
		if ((setjmp(env) == 0) && (depth < MAX_DEPTH)) {
			dive(depth + 1, mode, upper);
		} else {
			did_return[depth - 1] = -1;
			fprintf(stderr, "ascending(%d, %c, %p)\n", depth, "RLE"[mode], (void *)upper);
			longjmp(upper, 1);
		}
	}
	did_return[depth - 1] = 1;
	fprintf(stderr, "return(%d, %c, %p)\n", depth, "RLE"[mode], (void *)upper);
	return;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int setjmp_test(int argc, char *argv[])
#endif
{
	jmp_buf hook;
	int i;

	// Every depth of dive has returned.
	for (i = 0; i < MAX_DEPTH; ++i) {
		did_return[i] = 0;
	}
	assert(setjmp(hook) == 0);
	dive(1, DIVE_RETURN, hook);
	for (i = 0; i < MAX_DEPTH; ++i) {
		assert(did_return[i] = 1);
	}
	printf("setjmp does not get in the way or return\n");

	// Every depth "returned" via longjmp.
	for (i = 0; i < MAX_DEPTH; ++i) {
		did_return[i] = 0;
	}
	if (setjmp(hook) == 0) {
		dive(1, DIVE_LONGJMP, hook);
		assert(!"Not reached");
	}
	for (i = 0; i < MAX_DEPTH; ++i) {
		assert(did_return[i] = -1);
	}
	printf("setjmp skips a single stack frame\n");

	// Nothing returned (emergency ascent from the deep).
	for (i = 0; i < MAX_DEPTH; ++i) {
		did_return[i] = 0;
	}
	if (setjmp(hook) == 0) {
		dive(1, DIVE_EXCEPTION, hook);
		assert(!"Not reached");
	}
	for (i = 0; i < MAX_DEPTH - 1; ++i) {
		assert(did_return[i] == 0);
	}
	assert(did_return[MAX_DEPTH - 1] = -1);
	printf("setjmp skips multiple frames\n");

	return 0;
}
