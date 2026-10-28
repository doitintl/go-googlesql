package googlesql

import wasm2go "github.com/doitintl/googlesqlwasm2go"

// This file is hand-written in the DoiT fork (github.com/doitintl/go-googlesql)
// and is not part of the generated googlesql.go. Its entry points live in
// doit/doit_bridge.cc of the regeneration overlay (see REGENERATION.md),
// under service id 9000, which the generated bridge never reaches.
//
// Why: wasmify cannot carry absl::TimeZone across the Wasm boundary, so the
// generated SetDefaultTimeZone(*TimeZone) sets a default-constructed zone
// and DefaultTimeZone() returns an empty handle. These methods take and
// return the zone by name instead.

// SetDefaultTimeZoneName sets the analyzer's default time zone, used to fold
// zone-less TIMESTAMP literals and by functions that take an optional time
// zone, to the zone named by name: "UTC" or an IANA name such as
// "America/Los_Angeles", as GoogleSQL's own FindTimeZoneByName (absl::LoadTimeZone)
// accepts them; a fixed offset such as "+02:00" is not a name and is rejected.
// Named zones other than UTC are read from the host's zoneinfo directory.
// It is AnalyzerOptions::set_default_time_zone; an unknown name is an error
// and leaves the options unchanged. GoogleSQL's default is America/Los_Angeles.
func (h *AnalyzerOptions) SetDefaultTimeZoneName(name string) error {
	buf := pbAppendHandle(pbNewBuf(), 1, h.ptr)
	buf = pbAppendString(buf, 2, name)
	_, err := invokeMethod(9000, 0, buf, wasm2go.Inv_9000_0)
	return err
}

// DefaultTimeZoneName returns the name of the analyzer's default time zone
// (absl::TimeZone::name): "UTC" or an IANA name.
func (h *AnalyzerOptions) DefaultTimeZoneName() (string, error) {
	buf := pbAppendHandle(pbNewBuf(), 1, h.ptr)
	resp, err := invokeMethod(9000, 1, buf, wasm2go.Inv_9000_1)
	if err != nil {
		return "", err
	}
	return readScalarAtField(resp, 2, (*pbReader).readString), nil
}
