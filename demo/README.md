# Demo Data

Demo data for `make build-demo` and `./cmd/vibe-seed`. Theme: Parks and Rec.

## Files

| File | Format | Description |
|------|--------|-------------|
| `contacts.txt` | One name per line | Contact names (emails: name@pawnee.in.gov) |
| `notes.txt` | One title per line | Note titles |
| `tasks.txt` | One title per line | Task titles |
| `events.txt` | One title per line | Event titles |
| `theme.json` | JSON | Email domain, default note/event content |

## theme.json

```json
{
  "name": "Parks and Rec",
  "email_domain": "pawnee.in.gov",
  "note_content": "Demo content. Treat yo self.",
  "event_description": "Demo event. Everything is cccccool."
}
```

## Customizing

Edit the `.txt` files (one entry per line, blank lines ignored) and `theme.json` to change the demo theme. Run `make build-demo` to regenerate.
