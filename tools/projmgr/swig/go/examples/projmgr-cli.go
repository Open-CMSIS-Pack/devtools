package main

import (
	"projmgr"
	"os"
)

//go build -o cli projmgr-cli.go
func main() {

	projmgr.ProjMgrRunProjMgr(os.Args)

}
