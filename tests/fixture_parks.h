/* fixture_parks.h - Parks and Rec themed test data (configurable, random). */

#ifndef FIXTURE_PARKS_H
#define FIXTURE_PARKS_H

/* Seed storage with n dummy records (notes, tasks, contacts, events).
 * data_dir must already be initialized via storage_init(data_dir).
 * n is split roughly evenly across the four entity types; randomness
 * is controlled by VIBE_TEST_SEED (optional, for reproducibility). */
void fixture_seed_parks(const char *data_dir, int n);

#endif
