package lcm_gopacket

import (
	"encoding/binary"
	"fmt"
	"github.com/google/gopacket"
	"github.com/google/gopacket/layers"
)

const (
	lcmShortHeaderMagic      uint32 = 0x4c433032
	lcmFragmentedHeaderMagic uint32 = 0x4c433033
)

type LCMHeader struct {
	// COMMON FIELDS
	Magic          uint32
	SequenceNumber uint32
	// FRAGMENTED HEADER ONLY FIELDS
	Fragmented     bool
	PayloadSize    uint32
	FragmentOffset uint32
	FragmentNumber uint16
	TotalFragments uint16
	// COMMON FIELD
	ChannelName string
	// GOPACKET HELPER FIELDS
	fingerprint uint64
	contents    []byte
	payload     []byte
}

var LayerTypeLCMHeader gopacket.LayerType
var lcmLayerTypes map[uint64]gopacket.LayerType = map[uint64]gopacket.LayerType{}
var layerTypeIndex int = 1112

func Initialize() {
	metadata := gopacket.LayerTypeMetadata{Name: "LCMHeader",
		Decoder: gopacket.DecodeFunc(decodeLCMHeader)}

	LayerTypeLCMHeader = gopacket.RegisterLayerType(1111, metadata)

	layers.RegisterUDPPortLayerType(layers.UDPPort(7667), LayerTypeLCMHeader)
}

func RegisterLCMLayerType(fingerprint uint64, decoder gopacket.Decoder) gopacket.LayerType {
	name := fmt.Sprintf("%v", fingerprint)
	metadata := gopacket.LayerTypeMetadata{Name: name, Decoder: decoder}
	lcmLayerTypes[fingerprint] = gopacket.RegisterLayerType(layerTypeIndex, metadata)
	layerTypeIndex++

	return lcmLayerTypes[fingerprint]
}

func getLCMLayerType(fingerprint uint64) gopacket.LayerType {
	layerType, ok := lcmLayerTypes[fingerprint]
	if !ok {
		return gopacket.LayerTypePayload
	}

	return layerType
}

func decodeLCMHeader(data []byte, p gopacket.PacketBuilder) error {
	lcmHeader := &LCMHeader{}

	err := lcmHeader.DecodeFromBytes(data, p)
	if err != nil {
		return err
	}

	p.AddLayer(lcmHeader)
	p.SetApplicationLayer(lcmHeader)

	return p.NextDecoder(lcmHeader.NextLayerType())
}

func (lcm *LCMHeader) DecodeFromBytes(data []byte, df gopacket.DecodeFeedback) error {
	var offset int = 0

	lcm.Magic = binary.BigEndian.Uint32(data[:4])
	offset += 4

	if lcm.Magic != lcmShortHeaderMagic && lcm.Magic != lcmFragmentedHeaderMagic {
		return fmt.Errorf("Received LCM header magic %v does not match know "+
			"LCM magic numbers. Dropping packet.", lcm.Magic)
	}

	lcm.SequenceNumber = binary.BigEndian.Uint32(data[4:8])
	offset += 4

	if lcm.Magic == lcmFragmentedHeaderMagic {
		lcm.Fragmented = true

		lcm.PayloadSize = binary.BigEndian.Uint32(data[offset : offset+4])
		offset += 4

		lcm.FragmentOffset = binary.BigEndian.Uint32(data[offset : offset+4])
		offset += 4

		lcm.FragmentNumber = binary.BigEndian.Uint16(data[offset : offset+2])
		offset += 2

		lcm.TotalFragments = binary.BigEndian.Uint16(data[offset : offset+2])
		offset += 2
	} else {
		lcm.Fragmented = false
	}

	buffer := make([]byte, 0)
	for _, b := range data[8:] {
		offset++

		if b == 0 {
			break
		}

		buffer = append(buffer, b)
	}

	lcm.ChannelName = string(buffer)

	lcm.fingerprint = binary.BigEndian.Uint64(data[offset : offset+8])

	lcm.contents = data[:offset]
	lcm.payload = data[offset:]

	return nil
}

func (lcm LCMHeader) CanDecode() gopacket.LayerClass {
	return LayerTypeLCMHeader
}

func (lcm LCMHeader) NextLayerType() gopacket.LayerType {
	return getLCMLayerType(lcm.fingerprint)
}

func (lcm LCMHeader) LayerType() gopacket.LayerType {
	return LayerTypeLCMHeader
}

func (lcm LCMHeader) LayerContents() []byte {
	return lcm.contents
}

func (lcm LCMHeader) LayerPayload() []byte {
	return lcm.payload
}

func (lcm LCMHeader) Payload() []byte {
	return lcm.payload
}
