/* fixture_parks.c - Parks and Rec themed dummy data for tests. */

#include "fixture_parks.h"
#include "storage.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Simple LCG for deterministic tests when VIBE_TEST_SEED is set. */
static unsigned int fixture_rand(unsigned int *state) {
    *state = *state * 1103515245u + 12345u;
    return *state / 65536u % 32768u;
}

static int get_seed(void) {
    const char *s = getenv("VIBE_TEST_SEED");
    if (s && *s) {
        int n = atoi(s);
        return n ? n : 1;
    }
    return (int)time(NULL);
}

/* Pick one of the strings; count is array length (excluding NULL). */
static const char *pick(const char **arr, int count, unsigned int *state) {
    if (count <= 0) return "";
    return arr[fixture_rand(state) % (unsigned)count];
}

/* Parks and Rec inspired strings (short enough for VIBE_*_MAX). */

static const char *note_titles[] = {
    "Waffle Wednesday planning",
    "Ron's meat manifesto",
    "Eagleton rivalry notes",
    "Harvest Festival ideas",
    "Leslie's binder index",
    "JJ's Diner menu notes",
    "Treat Yo Self budget",
    "Mini horse adoption",
    "Pawnee parks survey",
    "Gryzzl app feedback",
    "Camping trip checklist",
    "Tom's business ideas",
    "Andy's band setlist",
    "April's internship log",
    "Ben's audit notes",
    "Chris's positivity list",
    "Donna's vacation plans",
    "Jerry's birthday list",
    "Li'l Sebastian memorial",
    "Snakehole Lounge ideas",
};

static const char *note_bodies[] = {
    "Never forget: waffles are the best breakfast food.",
    "Eagleton is the worst. We are the best.",
    "Lagavulin. Steak. Woodworking.",
    "Everything is fine. Literally the best.",
    "I have cried twice in my life. Once when I was seven and I was hit by a school bus.",
    "Treat yo self. Don't settle for less.",
    "Bye bye Li'l Sebastian. Miss you in the saddest fashion.",
    "Parks and rec forever.",
    "Government is the solution. Not the problem.",
};

static const char *task_titles[] = {
    "Organize Harvest Festival",
    "Buy Ron birthday present (meat)",
    "Draft memo for city manager",
    "Schedule town hall",
    "Order waffle supplies",
    "Update Pawnee website",
    "Plan Eagleton merger meeting",
    "Renew Lagavulin stock",
    "Call Ann about health fair",
    "Finish binder section 4",
    "Pick up Li'l Sebastian poster",
    "Tom's startup pitch prep",
    "April's Halloween party",
    "Donna's Treat Yo Self day",
};

static const char *due_dates[] = {
    "2025-03-15", "2025-04-01", "2025-05-20", "2026-01-10", "2025-12-31", ""
};

static const char *contact_names[] = {
    "Leslie Knope", "Ron Swanson", "April Ludgate", "Andy Dwyer",
    "Tom Haverford", "Donna Meagle", "Ben Wyatt", "Chris Traeger",
    "Ann Perkins", "Jerry Gergich", "Tommy Haverford", "Jean-Ralphio",
    "Li'l Sebastian", "Tammy Swanson", "Tammy Swanson 2", "Bobby Newport",
    "Councilman Jamm", "Joan Callamezzo", "Perd Hapley", "Orin",
};

static const char *contact_emails[] = {
    "leslie@pawnee.in.gov", "ron@parks.pawnee.in.gov", "april@parks.pawnee.in.gov",
    "andy@email.com", "tom@entertainment720.com", "donna@parks.pawnee.in.gov",
    "ben@city.pawnee.in.gov", "chris@city.pawnee.in.gov", "ann@nurse.net",
    "jerry@parks.pawnee.in.gov", "jeanralphio@email.com", "bobby@newport.com",
};

static const char *contact_phones[] = {
    "555-0100", "555-0101", "555-0102", "555-PARK", "555-E720", ""
};

static const char *event_titles[] = {
    "Harvest Festival", "Town Hall", "JJ's Diner lunch", "Waffle Wednesday",
    "Parks Dept meeting", "Eagleton summit", "Treat Yo Self day",
    "Li'l Sebastian memorial", "Camping trip", "Gryzzl launch",
    "Snakehole Lounge night", "Budget review", "Pawnee Goddesses",
    "Ron's cabin weekend", "Leslie's campaign kickoff",
};

static const char *event_times[] = {
    "2025-06-15 10:00:00", "2025-07-01 14:00:00", "2026-02-14 09:00:00",
    "2025-11-28 12:00:00", "2025-08-20 18:00:00",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

void fixture_seed_parks(const char *data_dir, int n) {
    unsigned int state = (unsigned int)get_seed();
    int per = n / 4;
    int extra = n % 4;
    int n_notes = per + (extra > 0 ? 1 : 0);
    int n_tasks = per + (extra > 1 ? 1 : 0);
    int n_contacts = per + (extra > 2 ? 1 : 0);
    int n_events = per;

    storage_init(data_dir);

    for (int i = 0; i < n_notes; i++) {
        const char *title = pick(note_titles, (int)COUNT(note_titles), &state);
        const char *body = pick(note_bodies, (int)COUNT(note_bodies), &state);
        storage_notes_add(title, body);
    }
    for (int i = 0; i < n_tasks; i++) {
        const char *title = pick(task_titles, (int)COUNT(task_titles), &state);
        const char *due = pick(due_dates, (int)COUNT(due_dates), &state);
        int prio = (int)(fixture_rand(&state) % 4);
        storage_tasks_add(title, due[0] ? due : NULL, prio);
    }
    for (int i = 0; i < n_contacts; i++) {
        const char *name = pick(contact_names, (int)COUNT(contact_names), &state);
        const char *email = pick(contact_emails, (int)COUNT(contact_emails), &state);
        const char *phone = pick(contact_phones, (int)COUNT(contact_phones), &state);
        storage_contacts_add(name, email, phone[0] ? phone : NULL, "");
    }
    for (int i = 0; i < n_events; i++) {
        const char *title = pick(event_titles, (int)COUNT(event_titles), &state);
        const char *start = pick(event_times, (int)COUNT(event_times), &state);
        const char *end = pick(event_times, (int)COUNT(event_times), &state);
        int all_day = (int)(fixture_rand(&state) % 2);
        storage_events_add(title, NULL, start, end, all_day);
    }
}
