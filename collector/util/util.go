package util

const TimeFormat = "2006-01-02 15:04:05 MST"

func YOrN(v bool) string {
	if v {
		return "Y"
	}
	return "N"
}
