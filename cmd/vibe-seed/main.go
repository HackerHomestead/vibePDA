// vibe-seed populates a database with demo data (Star Wars themed).
// Used by "make build-demo". Safe to run multiple times (inserts additional records).
package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"time"

	"github.com/you/vibe/internal/db"
)

const countPerType = 100

// Star Wars– themed demo data
var (
	contactNames = []string{
		"Luke Skywalker", "Leia Organa", "Han Solo", "Chewbacca", "Darth Vader",
		"Yoda", "Obi-Wan Kenobi", "R2-D2", "C-3PO", "Lando Calrissian",
		"Boba Fett", "Emperor Palpatine", "Darth Maul", "Qui-Gon Jinn", "Mace Windu",
		"Padmé Amidala", "Anakin Skywalker", "Count Dooku", "General Grievous", "Rey",
		"Finn", "Poe Dameron", "Kylo Ren", "BB-8", "Maz Kanata",
		"Captain Phasma", "Snoke", "Jyn Erso", "Cassian Andor", "K-2SO",
		"Chirrut Îmwe", "Baze Malbus", "Saw Gerrera", "Orson Krennic", "Director Krennic",
		"Admiral Ackbar", "Mon Mothma", "Wedge Antilles", "Biggs Darklighter", "Jek Tono Porkins",
		"Greedo", "Jabba the Hutt", "Bib Fortuna", "Salacious B. Crumb", "Wicket W. Warrick",
		"Logray", "Chief Chirpa", "Princess Kneesaa", "Admiral Piett", "Captain Needa",
		"Grand Moff Tarkin", "General Veers", "Admiral Ozzel", "Admiral Raddus", "Bodhi Rook",
		"Bail Organa", "Breha Organa", "Captain Panaka", "Sio Bibble", "Shmi Skywalker",
		"Cliegg Lars", "Owen Lars", "Beru Whitesun Lars", "Jar Jar Binks", "Boss Nass",
		"Captain Tarpals", "Nute Gunray", "Rune Haako", "Darth Tyranus", "Asajj Ventress",
		"Savage Opress", "Ahsoka Tano", "Captain Rex", "Commander Cody", "General Kenobi",
		"Ki-Adi-Mundi", "Plo Koon", "Kit Fisto", "Shaak Ti", "Aayla Secura",
		"Luminara Unduli", "Barriss Offee", "Jedi Master Yaddle", "Even Piell", "Depa Billaba",
		"Kanan Jarrus", "Ezra Bridger", "Sabine Wren", "Hera Syndulla", "Zeb Orrelios",
		"Chopper", "Grand Admiral Thrawn", "Agent Kallus", "Governor Pryce", "Grand Inquisitor",
		"Fifth Brother", "Seventh Sister", "Eighth Brother", "The Bendu", "Maul",
		"Dryden Vos", "Qi'ra", "Tobias Beckett", "Val", "Rio Durant",
		"Enfys Nest", "L3-37", "Darth Plagueis", "Darth Bane", "Revan",
		"Bastila Shan", "HK-47", "Kreia", "Atton Rand", "Visas Marr",
		"Mira", "Brianna", "Mical", "G0-T0", "Sion",
		"Nihilus", "Traya", "Meetra Surik", "Jolee Bindo", "Mission Vao",
		"Zaalbar", "Canderous Ordo", "Juhani", "Carth Onasi", "Zayne Carrick",
		"Mandalore the Ultimate", "Canderous Ordo", "Ulic Qel-Droma", "Exar Kun", "Nomi Sunrider",
	}

	noteTitles = []string{
		"Death Star plans analysis", "Jedi training notes", "Lightsaber maintenance log",
		"Hyperdrive specifications", "Trench run coordinates", "Force meditation techniques",
		"Rebel base locations", "Imperial fleet movements", "X-wing pre-flight checklist",
		"Millennium Falcon repairs", "Dagobah survival guide", "Carbonite freezing protocol",
		"Ewok language phrases", "Endor moon topography", "Second Death Star schematics",
		"Emperor's arrival schedule", "Shield generator specs", "AT-AT weak points",
		"Hoth evacuation plan", "Tauntaun handling tips", "Proton torpedo calibration",
		"Clone troop deployment", "Order 66 contingency", "Kamino cloning facility notes",
		"Geonosis battle plans", "Droid factory schematics", "Jedi Temple layout",
		"Coruscant underworld map", "Naboo palace security", "Gungan army tactics",
		"Pod racing modifications", "Mos Espa contacts", "Tatooine moisture farming",
		"Kessel Run calculations", "Spice trade routes", "Smuggler's code phrases",
		"Bounty hunter guild rules", "Mandalorian armor upkeep", "Beskar procurement",
		"Jabba's palace layout", "Rancor feeding schedule", "Sarlacc pit location",
		"Death Star II construction", "Executor bridge protocol", "Superlaser status",
		"Imperial march timing", "Stormtrooper accuracy stats", "TIE fighter specs",
		"Star Destroyer complement", "Executor dimensions", "Superweapon research",
		"Kyber crystal sources", "Starkiller base design", "First Order hierarchy",
		"Resistance intel summary", "Rey's training progress", "Luke's island notes",
		"Ahch-To temple ruins", "Porg population count", "Caretaking duties log",
		"Kylo Ren psychology", "Snoke's teachings", "Knights of Ren roster",
		"Exegol wayfinder data", "Sith wayfinder location", "Final Order fleet",
		"Palpatine resurrection", "Sith Eternal cult", "Ochi's mission log",
		"Sith dagger inscription", "Death Star wreckage map", "Pasaana festival dates",
		"Kijimi spice runner contacts", "Zorii Bliss terms", "Babu Frik services",
		"Scarif shield gate", "Rogue One mission brief", "Stardust plan details",
		"Galaxy's Edge expansion", "Star Tours route updates", "Lightsaber building guide",
		"Droid depot inventory", "Oga's cantina menu", "Black Spire outpost map",
		"Rise of the Resistance", "Smuggler's Run checklist", "Dok-Ondar's finds",
		"Savi's workshop reservations", "Droid depot parts", "Creature stall schedule",
		"Mandalorian covert locations", "Baby Yoda feeding log", "Razor Crest repairs",
		"Bounty puck priorities", "Guild dues status", "Imperial remnant intel",
		"Dark Trooper specs", "Moff Gideon targets", "Beskar shipment tracking",
		"Boba Fett return plan", "Tusken Raider diplomacy", "Sarlacc escape notes",
		"Palace takeover prep", "Rancor acquisition", "Krykna nest avoidance",
		"Old Republic archives", "Sith Empire history", "Jedi Civil War notes",
		"Mandalorian Wars timeline", "Star Forge location", "Rakata technology",
		"Korriban excavation", "Dantooine enclave", "Onderon rebellion",
		"Dxun moon wildlife", "Mandalorian supercommandos", "Mandalore siege plans",
		"Clone Wars timeline", "Separatist council", "Confederacy leadership",
		"Battle droid variants", "Super tactical droid", "Dark saber history",
		"Mandalorian Creed", "Way of the Mandalore", "Foundling protocol",
	}

	taskTitles = []string{
		"Destroy the Death Star", "Find R2-D2", "Rescue the princess",
		"Escape the trash compactor", "Disable tractor beam", "Return to Yavin",
		"Train with Yoda", "Confront Vader", "Save Han from Jabba",
		"Join the Rebellion", "Repair the Falcon", "Learn the Force",
		"Find Jedi Master Luke", "Defeat the Empire", "Restore the Republic",
		"Locate the plans", "Steal the data", "Deliver to Scarif",
		"Hold the line", "Destroy the shield", "Call for backup",
		"Evacuate Hoth", "Find new base", "Repair the ion cannon",
		"Defeat the AT-ATs", "Rescue the tauntaun", "Activate the shield",
		"Join the mission", "Trust the Force", "Use the targeting computer",
		"Follow Obi-Wan", "Learn from mistakes", "Save your friends",
		"Protect the younglings", "Face the dark side", "Redeem Anakin",
		"Complete training", "Pass the trials", "Build a lightsaber",
		"Find kyber crystal", "Meditate daily", "Study the archives",
		"Patrol the borders", "Report to command", "Inspect the fleet",
		"Attend the briefing", "Review intel", "Plan the attack",
		"Coordinate with squad", "Check the hyperdrive", "Calibrate the nav",
		"Stock the supplies", "Refuel the ship", "Arm the torpedoes",
		"Contact the base", "Encrypt the message", "Decode the transmission",
		"Scout the system", "Survey the planet", "Map the territory",
		"Negotiate with Jabba", "Pay off the bounty", "Clear the name",
		"Find the wayfinder", "Reach Exegol", "Rally the fleet",
		"Face Palpatine", "Destroy the Sith", "Balance the Force",
		"Rebuild the Jedi", "Start the academy", "Find new students",
		"Recover the artifact", "Decipher the map", "Follow the path",
		"Unite the clans", "Reclaim Mandalore", "Redeem the darksaber",
		"Protect the Child", "Find his kind", "Deliver the asset",
		"Escape the Imps", "Join the covert", "Earn the signet",
		"Complete the bounty", "Collect the puck", "Return to Nevarro",
		"Rebuild the Crest", "Upgrade the armor", "Find the Armorer",
		"Free the prisoner", "Expose the conspiracy", "Restore the honor",
		"Defeat Maul", "Liberate Mandalore", "Unify the system",
		"Survive Order 66", "Protect the younglings", "Find survivors",
		"Rebel against Empire", "Form the Alliance", "Plan the rebellion",
		"Sabotage the station", "Steal the shuttle", "Fake the clearance",
		"Infiltrate the base", "Extract the target", "Exfiltrate safely",
		"Secure the perimeter", "Hold the position", "Retreat when ordered",
		"Evacuate the civilians", "Protect the convoy", "Reach the ship",
		"Jump to hyperspace", "Avoid the Interdictor", "Lose the pursuit",
		"Rendezvous with fleet", "Debrief the mission", "File the report",
		"Rest and refit", "Tend the wounded", "Honor the fallen",
		"Prepare for battle", "Man the stations", "Ready the weapons",
		"Execute the plan", "Adapt to changes", "Improvise when needed",
		"Trust your crew", "Trust the Force", "Trust yourself",
	}

	eventTitles = []string{
		"Council meeting on Coruscant", "Training session on Dagobah", "Briefing on Yavin",
		"Attack on Death Star", "Battle of Hoth", "Battle of Endor",
		"Jabba's palace party", "Ewok celebration", "Medal ceremony",
		"Jedi Council session", "Senate hearing", "Trade Federation negotiation",
		"Pod race on Tatooine", "Lightsaber duel", "Fighter squadron briefing",
		"Scarif mission planning", "Rogue One deployment", "Data transfer extraction",
		"Shield gate assault", "Beach landing", "Archive tower infiltration",
		"Kessel Run attempt", "Smuggling run to Alderaan", "Cantina meetup",
		"Carbonite freezing", "Rescue from Jabba", "Sarlacc pit escape",
		"Emperor's arrival", "Throne room duel", "Death Star II destruction",
		"Clone deployment", "Kamino inspection", "Geonosis arena",
		"Droid factory raid", "First Battle of Geonosis", "Clone Wars start",
		"Order 66 execution", "Mustafar duel", "Temple assault",
		"Rebel Alliance founding", "Declaration of Rebellion", "Base relocation",
		"Starkiller base attack", "Resistance rally", "D'Qar evacuation",
		"Takodana siege", "Hosnian Prime destruction", "Starkiller firing",
		"Ahch-To training", "Luke's first lesson", "Care-taking duties",
		"Crait battle", "Salt speeder charge", "Luke's last stand",
		"Exegol assault", "Sith Eternal fleet", "Final battle",
		"Palpatine confrontation", "Rey and Ben", "Force dyad",
		"Tatooine sunset", "Binary sunset", "New hope",
		"Mandalorian bounty", "Nevarro guild meet", "Arvala-7 pickup",
		"Sorgan farming", "AT-ST assault", "Village defense",
		"Arvala quarry", "Mudhorn encounter", "Razor Crest repair",
		"Mos Eisley pickup", "Tatooine sand", "Stormtrooper patrol",
		"Nevarro covert", "Covert extraction", "Crest destruction",
		"Mos Pelgo marshall", "Krayt dragon hunt", "Tusken diplomacy",
		"Tython pilgrimage", "Seeing stone", "Dark troopers",
		"Moff Gideon capture", "Luke Skywalker arrival", "Grogu departure",
		"Boba Fett takeover", "Palace restoration", "Pyke syndicate",
		"Mod parlor visit", "Speed bike chase", "Rancor training",
		"Final showdown", "Cad Bane duel", "Pyke defeat",
		"Republic fair", "Bombing investigation", "Mandalore politics",
		"Night owl mission", "Imperial remnant", "Mandalorian fleet",
		"Space battle", "Planetary assault", "Siege warfare",
		"Jedi temple visit", "Archives research", "Ancient texts",
		"Sith holocron", "Dark side artifact", "Forbidden knowledge",
		"Hyperspace discovery", "Secret route", "Navigator meeting",
		"Smuggler's moon", "Black market deal", "Underworld contact",
		"Space station dock", "Crew rotation", "Supply run",
		"Diplomatic mission", "Peace talks", "Alliance building",
		"Intelligence briefing", "Spy report", "Covert ops",
		"Rescue mission", "Extraction plan", "Escape route",
		"Celebration day", "Liberation party", "Victory parade",
		"Memorial service", "Honor the fallen", "Remember the lost",
		"Promotion ceremony", "Medal of Bravery", "Distinguished service",
	}
)

func main() {
	dbPath := flag.String("db", "vibe-demo.db", "path to demo database (created in current dir)")
	flag.Parse()

	// Safety: only allow db path in current directory (never touch user's ~/.local/share/vibe/vibe.db)
	abs, err := filepath.Abs(*dbPath)
	if err != nil {
		log.Fatalf("resolve path: %v", err)
	}
	cwd, err := os.Getwd()
	if err != nil {
		log.Fatalf("getwd: %v", err)
	}
	cwdAbs, _ := filepath.Abs(cwd)
	if filepath.Dir(abs) != cwdAbs {
		log.Fatalf("refusing to use db outside current directory (would not be removed by make clean): %s", abs)
	}

	// Remove existing demo db so we start fresh each time
	_ = os.Remove(abs)

	dbDir := filepath.Dir(abs)
	if err := os.MkdirAll(dbDir, 0755); err != nil {
		log.Fatalf("create db dir: %v", err)
	}

	database, err := db.Open(abs)
	if err != nil {
		log.Fatalf("open db: %v", err)
	}
	defer database.Close()

	calendarRepo := db.NewCalendarRepo(database)
	tasksRepo := db.NewTasksRepo(database)
	notesRepo := db.NewNotesRepo(database)
	contactsRepo := db.NewContactsRepo(database)

	now := time.Now()
	baseDate := time.Date(now.Year(), now.Month(), 1, 0, 0, 0, 0, time.Local)

	// Seed contacts
	for i := 0; i < countPerType; i++ {
		name := contactNames[i%len(contactNames)]
		if i >= len(contactNames) {
			name = fmt.Sprintf("%s (%d)", name, i)
		}
		c := &db.Contact{
			Name:  name,
			Email: fmt.Sprintf("%s@starwars.galaxy", toEmailPart(name)),
			Phone: fmt.Sprintf("555-%04d", 1000+i%9000),
			Notes: "Demo contact",
		}
		if err := contactsRepo.Create(c); err != nil {
			log.Fatalf("create contact: %v", err)
		}
	}
	log.Printf("created %d contacts", countPerType)

	// Seed notes
	for i := 0; i < countPerType; i++ {
		title := noteTitles[i%len(noteTitles)]
		if i >= len(noteTitles) {
			title = fmt.Sprintf("%s (%d)", title, i)
		}
		n := &db.Note{
			Title:   title,
			Content: "Demo content. May the Force be with you.",
		}
		if err := notesRepo.Create(n); err != nil {
			log.Fatalf("create note: %v", err)
		}
	}
	log.Printf("created %d notes", countPerType)

	// Seed tasks
	for i := 0; i < countPerType; i++ {
		title := taskTitles[i%len(taskTitles)]
		if i >= len(taskTitles) {
			title = fmt.Sprintf("%s (%d)", title, i)
		}
		due := baseDate.AddDate(0, 0, i%60)
		t := &db.Task{
			Title:    title,
			Done:     i%5 == 0,
			DueDate:  &due,
			Priority: i % 5,
		}
		if err := tasksRepo.Create(t); err != nil {
			log.Fatalf("create task: %v", err)
		}
	}
	log.Printf("created %d tasks", countPerType)

	// Seed events (spread over a few months)
	for i := 0; i < countPerType; i++ {
		title := eventTitles[i%len(eventTitles)]
		if i >= len(eventTitles) {
			title = fmt.Sprintf("%s (%d)", title, i)
		}
		day := 1 + (i * 3) % 28
		month := int(baseDate.Month()) + (i/30)%3
		if month > 12 {
			month -= 12
		}
		year := baseDate.Year()
		if month < int(baseDate.Month()) {
			year++
		}
		start := time.Date(year, time.Month(month), day, 9+(i%8), (i*7)%60, 0, 0, time.Local)
		end := start.Add(time.Duration(1+i%3) * time.Hour)
		e := &db.Event{
			Title:       title,
			Description: "Demo event. The Force will be with you.",
			StartAt:     start,
			EndAt:       end,
			AllDay:      i%7 == 0,
		}
		if err := calendarRepo.Create(e); err != nil {
			log.Fatalf("create event: %v", err)
		}
	}
	log.Printf("created %d events", countPerType)

	log.Printf("demo database ready: %s", abs)
}

func toEmailPart(name string) string {
	var b []byte
	for _, r := range name {
		if (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9') {
			b = append(b, byte(r))
		} else if r == ' ' {
			b = append(b, '.')
		}
	}
	if len(b) == 0 {
		return "contact"
	}
	return string(b)
}
