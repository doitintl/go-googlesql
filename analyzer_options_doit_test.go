package googlesql_test

import (
	"strings"
	"testing"
	"time"

	"github.com/goccy/go-googlesql"
)

// literalMicros analyzes `SELECT TIMESTAMP '<text>'` and returns the folded
// literal as Unix microseconds.
func literalMicros(t *testing.T, text string, opts *googlesql.AnalyzerOptions, catalog *googlesql.SimpleCatalog, tf *googlesql.TypeFactory) int64 {
	t.Helper()
	out, err := googlesql.AnalyzeStatement("SELECT TIMESTAMP '"+text+"'", opts, catalog, tf)
	if err != nil {
		t.Fatalf("AnalyzeStatement: %v", err)
	}
	stmt, err := out.ResolvedStatement()
	if err != nil {
		t.Fatalf("ResolvedStatement: %v", err)
	}
	nodes, err := stmt.GetDescendantsWithKinds([]googlesql.ResolvedNodeKind{googlesql.ResolvedNodeKindResolvedLiteral})
	if err != nil {
		t.Fatalf("GetDescendantsWithKinds: %v", err)
	}
	if len(nodes) != 1 {
		t.Fatalf("got %d literals, want 1", len(nodes))
	}
	lit, ok := nodes[0].(*googlesql.ResolvedLiteral)
	if !ok {
		t.Fatalf("got %T, want *ResolvedLiteral", nodes[0])
	}
	v, err := lit.Value()
	if err != nil {
		t.Fatalf("Value: %v", err)
	}
	us, err := v.ToUnixMicros()
	if err != nil {
		t.Fatalf("ToUnixMicros: %v", err)
	}
	return us
}

// TestDefaultTimeZoneName covers the DoiT additions: the default zone is
// GoogleSQL's America/Los_Angeles until set, SetDefaultTimeZoneName("UTC")
// makes a zone-less TIMESTAMP literal fold to the UTC instant, the getter
// reports the name, and a bad name is an error that leaves the zone.
func TestDefaultTimeZoneName(t *testing.T) {
	t.Cleanup(gc)
	catalog, opts, tf, _ := setupAnalyzer(t)

	name, err := opts.DefaultTimeZoneName()
	if err != nil {
		t.Fatalf("DefaultTimeZoneName: %v", err)
	}
	if name != "America/Los_Angeles" {
		t.Fatalf("initial default time zone = %q, want America/Los_Angeles", name)
	}
	la, err := time.LoadLocation("America/Los_Angeles")
	if err != nil {
		t.Fatal(err)
	}
	wall := "2000-01-15 12:00:00"
	if got, want := literalMicros(t, wall, opts, catalog, tf), time.Date(2000, 1, 15, 12, 0, 0, 0, la).UnixMicro(); got != want {
		t.Fatalf("literal in default zone = %d, want %d (Los Angeles)", got, want)
	}

	if err := opts.SetDefaultTimeZoneName("UTC"); err != nil {
		t.Fatalf("SetDefaultTimeZoneName(UTC): %v", err)
	}
	if name, err = opts.DefaultTimeZoneName(); err != nil || name != "UTC" {
		t.Fatalf("DefaultTimeZoneName after set = %q, %v; want UTC", name, err)
	}
	for _, text := range []string{wall, "2000-07-15 12:00:00"} {
		ts, err := time.Parse("2006-01-02 15:04:05", text)
		if err != nil {
			t.Fatal(err)
		}
		if got, want := literalMicros(t, text, opts, catalog, tf), ts.UnixMicro(); got != want {
			t.Fatalf("TIMESTAMP '%s' folded to %d, want the UTC instant %d", text, got, want)
		}
	}

	if err := opts.SetDefaultTimeZoneName("Asia/Tokyo"); err != nil {
		t.Fatalf("SetDefaultTimeZoneName(Asia/Tokyo): %v", err)
	}
	if name, err = opts.DefaultTimeZoneName(); err != nil || name != "Asia/Tokyo" {
		t.Fatalf("DefaultTimeZoneName = %q, %v; want Asia/Tokyo", name, err)
	}
	// absl::LoadTimeZone takes zone names only; a fixed offset and an
	// unknown name are errors that leave the zone unchanged.
	for _, bad := range []string{"+02:00", "Mars/Olympus_Mons"} {
		err := opts.SetDefaultTimeZoneName(bad)
		if err == nil || !strings.Contains(err.Error(), "Invalid time zone") {
			t.Fatalf("SetDefaultTimeZoneName(%s) = %v, want an invalid time zone error", bad, err)
		}
		if name, gerr := opts.DefaultTimeZoneName(); gerr != nil || name != "Asia/Tokyo" {
			t.Fatalf("zone after SetDefaultTimeZoneName(%s) = %q, %v; want Asia/Tokyo", bad, name, gerr)
		}
	}
}
