package toast

// Msg is sent to the app to show a transient toaster message (e.g. "Deleted", "Saved").
type Msg struct {
	Text string
}
