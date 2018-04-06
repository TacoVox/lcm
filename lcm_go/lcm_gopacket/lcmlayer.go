package lcm_gopacket

import (
	"github.com/google/gopacket"
)

var lcmLayerTypes map[string]gopacket.LayerType

func RegisterLCMLayerType(channelName string, lt gopacket.LayerType) {
	lcmLayerTypes[channelName] = lt
}

func getLCMLayerType(channelName string) gopacket.LayerType {
	layerType, ok := lcmLayerTypes[channelName]
	if !ok {
		return gopacket.LayerTypePayload
	}

	return layerType
}
