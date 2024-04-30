/*
 * Copyright 2024, Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

typedef struct _dd_meta
{
    uint32_t threshold;
    uint32_t mango_pixels;
    uint32_t defect_pixels;
} dd_meta;

void *dd_meta_copy(void *user_data)
{
    dd_meta *dmeta = NULL;
    dd_meta *smeta = (dd_meta *) user_data;

    if (smeta) {
	dmeta = (dd_meta *) calloc(1, sizeof (dd_meta));

	dmeta->threshold = smeta->threshold;
	dmeta->mango_pixels = smeta->mango_pixels;
	dmeta->defect_pixels = smeta->defect_pixels;
    }

    return dmeta;
}

void dd_meta_release(void *user_data)
{
    if (user_data) {
        free(user_data);
    }

    user_data = NULL;
}
