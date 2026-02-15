/* main.c - vibePDA entry. Curses TUI (ncurses/PDCurses). Linux, DOS, WASM. */

#include "config.h"
#include "app.h"
#include "tui.h"
#include "vibe_config.h"
#include "storage.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(PLATFORM_LINUX)
#include <termios.h>
#include <unistd.h>
#endif

static void print_help(const char *prog) {
    printf("Usage: %s [OPTION]\n", prog);
    printf("       %s <entity> <action> [args...]   one-shot CLI (Linux/DOS)\n", prog);
    printf("Terminal personal data assistant (Notes, Tasks, Contacts, Calendar, Facts).\n\n");
    printf("Options:\n");
    printf("  -h, --help     display this help and exit\n");
    printf("  -v, --version  output version information and exit\n");
    printf("  --config       print current configuration and exit\n");
    printf("  --data-dir DIR override default data directory\n");
    printf("  --display MODE set display size: small (80x25), auto (terminal),\n");
    printf("                 custom COLxROW (e.g. --display custom 80x24)\n\n");
    printf("One-shot CLI (no TUI):\n");
    printf("  notes    add <title> [content] | list | show <id> | edit ... | delete <id>\n");
    printf("  tasks    add <title> [due_date] [priority] | list | show <id>\n");
    printf("           edit <id> ... | delete <id>\n");
    printf("  contacts add <name> [email] [phone] | list | show <id> | edit ... | delete\n");
    printf("  calendar add <title> [start] [end] [all_day] | list | show <id>\n");
    printf("           edit <id> ... | delete <id>\n");
    printf("  facts    add <key> <value> | list | show <id> | edit <id> <key> <value>\n");
    printf("           delete <id>\n");
    printf("  trash    list | restore <type> <id>   (type: note|task|contact|event|fact)\n\n");
    printf("  --mode tui             Terminal UI (default):\n");
    printf("                         F1 Help, F2 New, F3 Edit, F4 Delete\n");
    printf("  -cmd, --cmd, --mode command   Interactive command mode (REPL, like GW-BASIC):\n");
    printf("                                Type commands like 'notes list', 'tasks add ...'\n");
    printf("                                Use 'help' for available commands, 'quit' to exit\n\n");
    printf("TUI: Up/Down navigate, Tab switch pane, Enter edit, N new, D delete, ? help.\n");
    printf("     Content editor: Enter=newline, Enter+Enter=save, Esc=cancel, F5=line#.\n");
    printf("     Page Up/Down scrolls long notes. Arrow keys move cursor.\n");
}

static void print_version(void) {
    printf("%s %s\n", PACKAGE, VERSION);
}

/* Display mode: 0=auto (terminal size), 1=small (80x25), 2=custom (user COLxROW) */
#define DISPLAY_AUTO   0
#define DISPLAY_SMALL  1
#define DISPLAY_CUSTOM 2

static int display_mode = DISPLAY_AUTO;
static int display_cols = 80;
static int display_rows = 25;
static char *override_data_dir = NULL; /* User-specified data directory override */

/* Returns: 0=continue, 1=handled(help/version), 2=unknown argument error */
static int parse_args(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_help(argv[0]);
            return 1;
        }
        if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
            print_version();
            return 1;
        }
        if (strcmp(arg, "--config") == 0) {
            VibeConfig cfg;
            vibe_config_load(&cfg);
            if (override_data_dir) {
                snprintf(cfg.data_dir, sizeof(cfg.data_dir), "%s", override_data_dir);
            }
            vibe_config_print(&cfg);
            return 1;
        }
        if (strcmp(arg, "--data-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "unknown argument: %s (missing directory)\n", arg);
                print_help(argv[0]);
                return 2;
            }
            override_data_dir = argv[++i];
            continue;
        }
        if (strcmp(arg, "-cmd") == 0 || strcmp(arg, "--cmd") == 0)
            continue;
        if (strcmp(arg, "--mode") == 0) {
            if (i + 1 < argc && (strcmp(argv[i + 1], "tui") == 0 || strcmp(argv[i + 1], "command") == 0)) {
                i++;
                continue;
            }
            fprintf(stderr, "unknown argument: %s\n", i + 1 < argc ? argv[i + 1] : arg);
            print_help(argv[0]);
            return 2;
        }
        if (strcmp(arg, "--display") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "unknown argument: %s (missing value)\n", arg);
                print_help(argv[0]);
                return 2;
            }
            const char *val = argv[++i];
            if (strcmp(val, "small") == 0) {
                display_mode = DISPLAY_SMALL;
                display_cols = 80;
                display_rows = 25;
            } else if (strcmp(val, "auto") == 0) {
                display_mode = DISPLAY_AUTO;
            } else if (strcmp(val, "custom") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "unknown argument: %s (missing COLxROW)\n", arg);
                    print_help(argv[0]);
                    return 2;
                }
                val = argv[++i];
                int c = 0, r = 0;
                if (sscanf(val, "%dx%d", &c, &r) == 2 &&
                    c >= 24 && c <= 256 && r >= 10 && r <= 100) {
                    display_mode = DISPLAY_CUSTOM;
                    display_cols = c;
                    display_rows = r;
                } else {
                    fprintf(stderr, "unknown argument: %s (invalid COLxROW)\n", val);
                    print_help(argv[0]);
                    return 2;
                }
            } else {
                fprintf(stderr, "unknown argument: %s (expected small|auto|custom)\n", val);
                print_help(argv[0]);
                return 2;
            }
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "unknown argument: %s\n", arg);
            print_help(argv[0]);
            return 2;
        }
    }
    return 0;
}

/* Get display dimensions for app. Call after tui_init() for auto mode. */
static void get_display_dimensions(int *rows, int *cols) {
    if (display_mode == DISPLAY_AUTO) {
        *rows = tui_rows();
        *cols = tui_cols();
        /* Clamp to reasonable minimum */
        if (*rows < 10) *rows = 10;
        if (*cols < 24) *cols = 24;
    } else {
        *rows = display_rows;
        *cols = display_cols;
    }
}

#if defined(PLATFORM_LINUX) || defined(PLATFORM_DOS)
static int run_cli(int argc, char **argv); /* forward */

/* Return 1 if interactive command mode was requested (-cmd, --cmd, --mode command). */
static int want_interactive_cmd(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-cmd") == 0 || strcmp(argv[i], "--cmd") == 0 ||
            strcmp(argv[i], "--mode") == 0) {
            if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc && strcmp(argv[i + 1], "command") == 0)
                return 1;
            if (strcmp(argv[i], "-cmd") == 0 || strcmp(argv[i], "--cmd") == 0)
                return 1;
        }
    }
    return 0;
}

#define CMD_LINE_MAX  512
#define CMD_TOKENS_MAX 32
#define HISTORY_MAX   128

#if defined(PLATFORM_LINUX)
/* Read a line with Up/Down history. prompt is e.g. "> ".
 * history[*history_cur] is shown when navigating; caller sets *history_cur = *history_len before call.
 * Returns 1 if a line was read, 0 on EOF. */
static int read_line_with_history(char *buf, int size, const char *prompt,
    char (*history)[CMD_LINE_MAX], int *history_len, int *history_cur) {
    struct termios old, raw;
    int len = 0, c, ret = 1;
    if (!isatty(STDIN_FILENO) || size < 1) return 0;
    buf[0] = '\0';
    if (tcgetattr(STDIN_FILENO, &old) != 0) return 0;
    raw = old;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return 0;

    for (;;) {
        c = getchar();
        if (c == EOF) { ret = 0; break; }
        if (c == '\n' || c == '\r') {
            buf[len] = '\0';
            break;
        }
        if (c == 127 || c == 8) { /* Backspace */
            if (len > 0) {
                len--;
                buf[len] = '\0';
                printf("\r\033[K%s%s", prompt, buf);
                fflush(stdout);
            }
            continue;
        }
        if (c == 27) { /* ESC: read CSI or similar */
            int c2 = getchar();
            if (c2 == EOF) break;
            if (c2 == '[' || c2 == 'O') {
                int c3 = getchar();
                if (c3 == 'A') { /* Up */
                    if (*history_len > 0 && *history_cur > 0) {
                        (*history_cur)--;
                        (void)snprintf(buf, (size_t)size, "%s", history[*history_cur]);
                        len = (int)strlen(buf);
                        printf("\r\033[K%s%s", prompt, buf);
                        fflush(stdout);
                    }
                } else if (c3 == 'B') { /* Down */
                    if (*history_cur < *history_len) {
                        (*history_cur)++;
                        if (*history_cur == *history_len) {
                            buf[0] = '\0';
                            len = 0;
                        } else {
                            (void)snprintf(buf, (size_t)size, "%s", history[*history_cur]);
                            len = (int)strlen(buf);
                        }
                        printf("\r\033[K%s%s", prompt, buf);
                        fflush(stdout);
                    }
                }
            }
            continue;
        }
        if (c >= 32 && len < size - 1) {
            buf[len++] = (char)c;
            buf[len] = '\0';
            putchar((char)c);
            fflush(stdout);
        }
    }

    (void)tcsetattr(STDIN_FILENO, TCSANOW, &old);
    if (ret && len == 0) return 1; /* empty line */
    return ret;
}
#endif

/* Tokenize line into argv (in-place); return number of tokens. Supports "quoted strings". */
static int tokenize_line(char *line, char **tokens, int max_tok) {
    int n = 0;
    char *p = line;
    while (n < max_tok) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        if (*p == '"') {
            p++;
            tokens[n++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
            continue;
        }
        tokens[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        if (!*p) break;
        *p++ = '\0';
    }
    return n;
}

static void run_interactive_cmd_mode(void) {
    VibeConfig cfg;
    vibe_config_load(&cfg);
    if (override_data_dir) {
        snprintf(cfg.data_dir, sizeof(cfg.data_dir), "%s", override_data_dir);
    }
    storage_init(cfg.data_dir);

#if defined(PLATFORM_LINUX)
    static char history[HISTORY_MAX][CMD_LINE_MAX];
    static int history_len;
    int history_cur;
    int use_history = isatty(STDIN_FILENO);
#else
    (void)0;
#endif

    printf("%s %s — interactive command mode (↑/↓ history, 'help', 'clear', 'quit')\n", PACKAGE, VERSION);
    printf("> ");
    fflush(stdout);

    for (;;) {
        char line[CMD_LINE_MAX];
        char *tokens[CMD_TOKENS_MAX];
        char *argv_buf[CMD_TOKENS_MAX + 1];
        int n, code;

#if defined(PLATFORM_LINUX)
        if (use_history) {
            history_cur = history_len;
            if (!read_line_with_history(line, sizeof(line), "> ", history, &history_len, &history_cur)) {
                if (feof(stdin)) break;
                continue;
            }
        } else
#endif
        {
            if (!fgets(line, sizeof(line), stdin)) {
                if (feof(stdin)) break;
                continue;
            }
            line[strcspn(line, "\n\r")] = '\0';
        }

        /* Newline so response (help, command output) starts on next line */
        printf("\n");

        /* Append to history (non-empty, not duplicate of last) */
#if defined(PLATFORM_LINUX)
        if (use_history && line[0] != '\0') {
            if (history_len == 0 || strcmp(line, history[history_len - 1]) != 0) {
                if (history_len < HISTORY_MAX) {
                    (void)snprintf(history[history_len], CMD_LINE_MAX, "%s", line);
                    history_len++;
                } else {
                    int i;
                    for (i = 0; i < HISTORY_MAX - 1; i++)
                        (void)memmove(history[i], history[i + 1], CMD_LINE_MAX);
                    (void)snprintf(history[HISTORY_MAX - 1], CMD_LINE_MAX, "%s", line);
                }
            }
        }
#endif

        n = tokenize_line(line, tokens, CMD_TOKENS_MAX);
        if (n == 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }

        if (strcmp(tokens[0], "quit") == 0 || strcmp(tokens[0], "exit") == 0 || strcmp(tokens[0], "q") == 0)
            break;

        if (strcmp(tokens[0], "help") == 0) {
            printf("notes [add|list|show|edit|delete] ...\n");
            printf("tasks [add|list|show|delete] ...\n");
            printf("contacts [add|list|delete] ...\n");
            printf("calendar [add|list|delete] ...\n");
            printf("trash [list|restore] ...\n");
            printf("list — show all records in table format\n");
            printf("clear — clear screen\n");
            printf("↑/↓ — command history\n");
            printf("quit, exit, q — exit\n");
            printf("> ");
            fflush(stdout);
            continue;
        }

        if (strcmp(tokens[0], "clear") == 0 || strcmp(tokens[0], "cls") == 0) {
            printf("\033[2J\033[H");
            fflush(stdout);
            printf("> ");
            fflush(stdout);
            continue;
        }

        if (strcmp(tokens[0], "list") == 0) {
            printf("NOTES\n");
            printf("ID\tTitle\tContent\n");
            void print_note(const VibeNote *n, void *ctx) {
                (void)ctx;
                char content_preview[32];
                int len = (int)strlen(n->content);
                if (len > 30) {
                    memcpy(content_preview, n->content, 27);
                    content_preview[27] = '.';
                    content_preview[28] = '.';
                    content_preview[29] = '.';
                    content_preview[30] = '\0';
                } else {
                    strcpy(content_preview, n->content);
                }
                printf("%d\t%s\t%s\n", n->id, n->title, content_preview);
            }
            storage_notes_list(print_note, NULL);
            printf("\nTASKS\n");
            printf("ID\tTitle\tDue\tDone\n");
            void print_task(const VibeTask *t, void *ctx) {
                (void)ctx;
                printf("%d\t%s\t%s\t%d\n", t->id, t->title, t->due_date, t->done);
            }
            storage_tasks_list(print_task, NULL);
            printf("\nCONTACTS\n");
            printf("ID\tName\tEmail\tPhone\n");
            void print_contact(const VibeContact *c, void *ctx) {
                (void)ctx;
                printf("%d\t%s\t%s\t%s\n", c->id, c->name, c->email, c->phone);
            }
            storage_contacts_list(print_contact, NULL);
            printf("\nCALENDAR\n");
            printf("ID\tTitle\tStart\tEnd\n");
            void print_event(const VibeCalendarEvent *e, void *ctx) {
                (void)ctx;
                printf("%d\t%s\t%s\t%s\n", e->id, e->title, e->start_at, e->end_at);
            }
            storage_events_list(print_event, NULL);
            printf("> ");
            fflush(stdout);
            continue;
        }

        argv_buf[0] = "vibe";
        for (int i = 0; i < n; i++) argv_buf[i + 1] = tokens[i];
        code = run_cli(n + 1, argv_buf);
        if (code == -1) {
            printf("Unknown command '%s'. Type 'help'.\n", tokens[0]);
        } else if (code != 0) {
            /* run_cli already printed to stderr */
        }

        printf("> ");
        fflush(stdout);
    }
}
#endif

#if defined(PLATFORM_LINUX) || defined(PLATFORM_DOS)
static int run_cli(int argc, char **argv) {
    if (argc < 2) return -1;
    /* Skip over known options that don't cause early exit */
    int arg_idx = 1;
    while (arg_idx < argc) {
        const char *arg = argv[arg_idx];
        if (strcmp(arg, "--data-dir") == 0) {
            arg_idx += 2; /* Skip --data-dir and its value */
            continue;
        }
        if (strcmp(arg, "--display") == 0) {
            arg_idx += 2; /* Skip --display and its value */
            if (arg_idx < argc && strcmp(argv[arg_idx - 1], "custom") == 0) {
                arg_idx++; /* Skip COLxROW for custom display */
            }
            continue;
        }
        if (strcmp(arg, "--mode") == 0 || strcmp(arg, "-cmd") == 0 || strcmp(arg, "--cmd") == 0) {
            arg_idx++;
            if (arg_idx < argc && (strcmp(argv[arg_idx], "tui") == 0 || strcmp(argv[arg_idx], "command") == 0)) {
                arg_idx++;
            }
            continue;
        }
        break; /* Found non-option argument, this should be the entity */
    }
    if (arg_idx >= argc) return -1;
    const char *entity = argv[arg_idx];
    const char *action = arg_idx + 1 < argc ? argv[arg_idx + 1] : "";
    /* Entity-only (e.g. "notes") defaults to "list" for interactive mode */
    if (action[0] == '\0' && (strcmp(entity, "notes") == 0 || strcmp(entity, "tasks") == 0 ||
        strcmp(entity, "contacts") == 0 || strcmp(entity, "calendar") == 0 || strcmp(entity, "trash") == 0))
        action = "list";

    VibeConfig cfg;
    vibe_config_load(&cfg);
    if (override_data_dir) {
        snprintf(cfg.data_dir, sizeof(cfg.data_dir), "%s", override_data_dir);
    }
    storage_init(cfg.data_dir);

    if (strcmp(entity, "notes") == 0) {
        if (strcmp(action, "add") == 0) {
            const char *title = arg_idx + 2 < argc ? argv[arg_idx + 2] : "";
            const char *content = arg_idx + 3 < argc ? argv[arg_idx + 3] : "";
            int id = storage_notes_add(title, content);
            if (id) { printf("%d\n", id); return 0; }
            return 1;
        }
        if (strcmp(action, "list") == 0) {
            void print_note(const VibeNote *n, void *ctx) {
                (void)ctx;
                printf("%d\t%s\t%s\n", n->id, n->title, n->content);
            }
            storage_notes_list(print_note, NULL);
            return 0;
        }
        if (strcmp(action, "show") == 0 && arg_idx + 2 < argc) {
            int id = atoi(argv[arg_idx + 2]);
            VibeNote n;
            if (storage_note_get(id, &n)) {
                printf("id: %d\ntitle: %s\ncontent: %s\ncreated: %s\n", n.id, n.title, n.content, n.created_at);
                return 0;
            }
            fprintf(stderr, "Note %d not found\n", id);
            return 1;
        }
        if (strcmp(action, "edit") == 0 && arg_idx + 3 < argc) {
            int id = atoi(argv[arg_idx + 2]);
            const char *title = argv[arg_idx + 3];
            const char *content = arg_idx + 4 < argc ? argv[arg_idx + 4] : "";
            if (storage_notes_update(id, title, content)) { printf("ok\n"); return 0; }
            return 1;
        }
        if (strcmp(action, "delete") == 0 && arg_idx + 2 < argc) {
            int id = atoi(argv[arg_idx + 2]);
            if (storage_notes_delete(id)) { printf("ok\n"); return 0; }
            return 1;
        }
    }

    if (strcmp(entity, "tasks") == 0) {
        if (strcmp(action, "add") == 0) {
            const char *title = arg_idx + 2 < argc ? argv[arg_idx + 2] : "";
            const char *due = arg_idx + 3 < argc ? argv[arg_idx + 3] : "";
            int prio = arg_idx + 4 < argc ? atoi(argv[arg_idx + 4]) : 0;
            int id = storage_tasks_add(title, due, prio);
            if (id) { printf("%d\n", id); return 0; }
            return 1;
        }
        if (strcmp(action, "list") == 0) {
            void print_task(const VibeTask *t, void *ctx) {
                (void)ctx;
                printf("%d\t%s\t%s\t%d\n", t->id, t->title, t->due_date, t->done);
            }
            storage_tasks_list(print_task, NULL);
            return 0;
        }
        if (strcmp(action, "show") == 0 && arg_idx + 2 < argc) {
            int id = atoi(argv[arg_idx + 2]);
            VibeTask t;
            if (storage_task_get(id, &t)) {
                printf("id: %d\ntitle: %s\ndue: %s\ndone: %d\npriority: %d\n", t.id, t.title, t.due_date, t.done, t.priority);
                return 0;
            }
            fprintf(stderr, "Task %d not found\n", id);
            return 1;
        }
        if (strcmp(action, "delete") == 0 && arg_idx + 2 < argc) {
            int id = atoi(argv[arg_idx + 2]);
            if (storage_tasks_delete(id)) { printf("ok\n"); return 0; }
            return 1;
        }
    }

    if (strcmp(entity, "contacts") == 0) {
        if (strcmp(action, "add") == 0) {
            const char *name = arg_idx + 2 < argc ? argv[arg_idx + 2] : "";
            const char *email = arg_idx + 3 < argc ? argv[arg_idx + 3] : "";
            const char *phone = arg_idx + 4 < argc ? argv[arg_idx + 4] : "";
            int id = storage_contacts_add(name, email, phone);
            if (id) { printf("%d\n", id); return 0; }
            return 1;
        }
        if (strcmp(action, "list") == 0) {
            void print_contact(const VibeContact *c, void *ctx) {
                (void)ctx;
                printf("%d\t%s\t%s\t%s\n", c->id, c->name, c->email, c->phone);
            }
            storage_contacts_list(print_contact, NULL);
            return 0;
        }
        if (strcmp(action, "delete") == 0 && arg_idx + 2 < argc) {
            int id = atoi(argv[arg_idx + 2]);
            if (storage_contacts_delete(id)) { printf("ok\n"); return 0; }
            return 1;
        }
    }

    if (strcmp(entity, "calendar") == 0) {
        if (strcmp(action, "add") == 0) {
            const char *title = arg_idx + 2 < argc ? argv[arg_idx + 2] : "";
            const char *start = arg_idx + 3 < argc ? argv[arg_idx + 3] : "";
            const char *end = arg_idx + 4 < argc ? argv[arg_idx + 4] : "";
            int all_day = arg_idx + 5 < argc ? atoi(argv[arg_idx + 5]) : 0;
            int id = storage_events_add(title, NULL, start, end, all_day);
            if (id) { printf("%d\n", id); return 0; }
            return 1;
        }
        if (strcmp(action, "list") == 0) {
            void print_event(const VibeCalendarEvent *e, void *ctx) {
                (void)ctx;
                printf("%d\t%s\t%s\t%s\n", e->id, e->title, e->start_at, e->end_at);
            }
            storage_events_list(print_event, NULL);
            return 0;
        }
        if (strcmp(action, "delete") == 0 && arg_idx + 2 < argc) {
            int id = atoi(argv[arg_idx + 2]);
            if (storage_events_delete(id)) { printf("ok\n"); return 0; }
            return 1;
        }
    }

    if (strcmp(entity, "trash") == 0) {
        if (strcmp(action, "list") == 0) {
            void print_trash(int type, int id, const char *title, void *ctx) {
                (void)ctx;
                const char *t = (type == 0) ? "note" : (type == 1) ? "task" : (type == 2) ? "contact" : "event";
                printf("%s\t%d\t%s\n", t, id, title);
            }
            storage_trash_list(print_trash, NULL);
            return 0;
        }
        if (strcmp(action, "restore") == 0 && arg_idx + 3 < argc) {
            int type = (strcmp(argv[arg_idx + 2], "note") == 0) ? 0 : (strcmp(argv[arg_idx + 2], "task") == 0) ? 1 : (strcmp(argv[arg_idx + 2], "contact") == 0) ? 2 : 3;
            int id = atoi(argv[arg_idx + 3]);
            if (storage_restore(type, id)) { printf("ok\n"); return 0; }
            return 1;
        }
    }

    return -1; /* not a CLI command, run TUI */
}
#endif

int main(int argc, char **argv) {
    int pr = parse_args(argc, argv);
    if (pr == 1) return 0;
    if (pr == 2) return 1;

#if defined(PLATFORM_LINUX) || defined(PLATFORM_DOS)
    if (want_interactive_cmd(argc, argv)) {
        run_interactive_cmd_mode();
        return 0;
    }
    if (argc >= 2) {
        int code = run_cli(argc, argv);
        if (code >= 0) return code;
    }
#endif

    {
        VibeConfig cfg;
        vibe_config_load(&cfg);
        if (override_data_dir) {
            snprintf(cfg.data_dir, sizeof(cfg.data_dir), "%s", override_data_dir);
        }
        storage_init(cfg.data_dir);
        (void)cfg;
    }

    tui_init();

    {
        int rows, cols;
        get_display_dimensions(&rows, &cols);
        AppState app;
        app_init(&app, rows, cols);

        for (;;) {
            app_draw(&app);
            int key = tui_getkey();
#ifdef KEY_RESIZE
            if (key == KEY_RESIZE && display_mode == DISPLAY_AUTO) {
                get_display_dimensions(&app.rows, &app.cols);
                continue;
            }
#endif
            app_handle_key(&app, key);
            if (app.quit_requested)
                break;
        }
    }

    tui_cleanup();
    return 0;
}
