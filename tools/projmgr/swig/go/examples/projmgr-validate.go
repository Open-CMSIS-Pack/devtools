package main

import (
	"fmt"
	"os"
	"projmgr"
)

//go build -o validate projmgr-validate.go
func main() {

	manager := projmgr.NewProjMgr()
	parser := manager.GetParser()

	if parser.ParseCsolution(os.Args[1]) == true {
		fmt.Println(os.Args[1] + " was parsed successfully")
	}

}
