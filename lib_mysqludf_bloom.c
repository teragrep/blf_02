/*
 * Copyright 2012, XENEI.com
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(WIN32)
#define DLLEXP __declspec(dllexport)
#else
#define DLLEXP
#endif

#include <string.h>

#include <mysql.h>
#include <ctype.h>

#include "config.h"
#include <stdio.h>

#include <stdint.h>
#include <smmintrin.h>
#include <immintrin.h>

/* For Windows, define PACKAGE_STRING in the VS project */
#ifndef __WIN__
#include "config.h"
#endif

#ifndef my_bool
typedef char my_bool;
#endif

/* These must be right or mysqld will not find the symbol! */
#ifdef	__cplusplus
extern "C" {
#endif
	DLLEXP my_bool bloommatch_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
	DLLEXP void bloommatch_deinit(UDF_INIT *initid);
	DLLEXP my_bool bloommatch(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);

	DLLEXP my_bool bloomupdate_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
	DLLEXP void bloomupdate_deinit(UDF_INIT *initid);
	DLLEXP char* bloomupdate(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);

#ifdef	__cplusplus
}
#endif


my_bool bloommatch_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	/* make sure user has provided exactly two string arguments */
	if (args->arg_count != 2 || (args->arg_type[0] != STRING_RESULT)
							 || (args->arg_type[1] != STRING_RESULT)){
		strcpy(message, "bloommatch requires 2 blob or string arguments");
		return 1;
	}

	return 0;
}

void bloommatch_deinit(UDF_INIT *initid)
{
}


my_bool bloommatch(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error)
{
    if (args->lengths[0] > args->lengths[1])
        {
            return 0;
        }

    char *first_array = args->args[0];
    char *second_array = args->args[1];
    int limit_a = args->lengths[0];
    int limit_b = args->lengths[1];
    
    if (limit_a != limit_b)
        {
            return 0;
        }

    int i = 0;

    // load 32 byte chunks and process them with avx2 instructions
    int chunks = limit_a / 32;
    for (i = 0; i < chunks * 32; i += 32)
        {
            // load 32 bytes from each of the arrays with loadu (unaligned memory access)
            __m256i vec_a = _mm256_loadu_si256((__m256i *)&first_array[i]);
            __m256i vec_b = _mm256_loadu_si256((__m256i *)&second_array[i]);

            // perform bitwise and which results into a mask
            __m256i result = _mm256_and_si256(vec_a, vec_b);

            // compare mask with the requested match
            __m256i cmp = _mm256_cmpeq_epi8(result, vec_a);

            // verify that all bytes are set (0xFFFFFFFF, or -1 as a signed integer) or filter does not match
            if (_mm256_movemask_epi8(cmp) != -1)
                {
                    return 0;
                }
        }

    // process reminder of the bytes, that did not fit into 32 byte chunks
    for (; i < limit_a; i++)
        {
            unsigned char a = (unsigned char)first_array[i];
            unsigned char b = (unsigned char)second_array[i];
            if ((a & b) != a)
                {
                    return 0;
                }
        }

    return 1;
}

my_bool bloomupdate_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	/* make sure user has provided exactly two string arguments */
	if (args->arg_count != 2 || (args->arg_type[0] != STRING_RESULT)
							 || (args->arg_type[1] != STRING_RESULT)){
		strcpy(message, "bloomupdate requires 2 blob or string arguments");
		return 1;
	}

	args->maybe_null[0] = 1;
	args->maybe_null[1] = 1;

	initid->ptr = malloc( initid->max_length);
	if (initid->ptr == 0)
	{
		strcpy(message, "bloomupdate not enough memory for buffer");
		return 1;
	}

	return 0;
}

void bloomupdate_deinit(UDF_INIT *initid)
{
	if (initid->ptr != 0)
	{
		free( initid->ptr);
	}
}


char* bloomupdate(UDF_INIT *initid, UDF_ARGS *args, char* result, unsigned long* length,	char *is_null, char *error)
{
	char* a=args->args[0];
	char* b=args->args[1];

	int alimit = a==NULL?0:args->lengths[0];
	int blimit = b==NULL?0:args->lengths[1];
	int limit = alimit>blimit ? alimit : blimit;

	int i;

	for (i=0;i<limit;i++)
	{
		if (alimit <= i)
		{
			initid->ptr[i] = b[i];
		} else {
			if (blimit <= i)
			{
				initid->ptr[i] = a[i];
			} else {
				initid->ptr[i] = a[i] | b[i] ;
			}
		}
	}
	*length = limit;
	return initid->ptr;
}
