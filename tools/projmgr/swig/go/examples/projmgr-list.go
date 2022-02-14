package main

import (
	"fmt"
	"projmgr"
)

//go build -o list projmgr-list.go
func main() {

	manager := projmgr.NewProjMgr()
	worker := manager.GetWorker()

	packVector := projmgr.NewStringVector()
	worker.ListPacks("ARM", packVector)
	if !packVector.IsEmpty() {
		fmt.Println("\nInstalled 'ARM' packs:")
		for i := 0; i < int(packVector.Size()); i++ {
		fmt.Println(packVector.Get(i))
		}
	}

	deviceVector := projmgr.NewStringVector()
	worker.ListDevices("ARM", deviceVector)
	if !deviceVector.IsEmpty() {
		fmt.Println("\nInstalled 'ARM' devices:")
		for i := 0; i < int(deviceVector.Size()); i++ {
		fmt.Println(deviceVector.Get(i))
		}
	}

}
