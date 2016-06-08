/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef _GPT_CRC32_H_
#define _GPT_CRC32_H_

#include <inttypes.h>  /* For PRIu64 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(HAVE_ENDIAN_H) && defined(HAVE_LITTLE_ENDIAN)
#include <byteswap.h>
#include <memory.h>
#endif

uint32_t calc_crc32(const void *buffer, uint32_t len);

#endif  // _GPT_CRC32_H_
