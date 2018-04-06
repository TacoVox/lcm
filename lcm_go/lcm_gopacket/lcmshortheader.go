package lcm_gopacket

import (
	"encoding/binary"
	"github.com/google/gopacket"
)

type LCMShortHeader struct {
	Magic          uint32
	SequenceNumber uint32
	ChannelName    string
	contents       []byte
	payload        []byte
}

var LayerTypeLCMShort gopacket.LayerType

func init() {
	metadata := gopacket.LayerTypeMetadata{Name: "LCMShortHeader",
		Decoder: gopacket.DecodeFunc(decodeLCMShortHeader)}

	LayerTypeLCMShort = gopacket.RegisterLayerType(1111, metadata)
}

func decodeLCMShortHeader(data []byte, p gopacket.PacketBuilder) error {
	lcmShortHeader := &LCMShortHeader{}

	err := lcmShortHeader.DecodeFromBytes(data, p)
	if err != nil {
		return err
	}

	p.AddLayer(lcmShortHeader)
	p.SetApplicationLayer(lcmShortHeader)

	return p.NextDecoder(lcmShortHeader.NextLayerType())
}

func (lcm *LCMShortHeader) DecodeFromBytes(data []byte, df gopacket.DecodeFeedback) error {
	var offset int = 0

	lcm.Magic = binary.BigEndian.Uint32(data[:4])
	offset += 4

	lcm.SequenceNumber = binary.BigEndian.Uint32(data[4:8])
	offset += 4

	buffer := make([]byte, 1)
	for _, b := range data[8:] {
		offset++

		if b == 0 {
			break
		}

		buffer = append(buffer, b)
	}
	lcm.ChannelName = string(buffer)

	lcm.contents = data[:offset]
	lcm.payload = data[offset:]

	return nil
}

func (lcm LCMShortHeader) CanDecode() gopacket.LayerClass {
	return LayerTypeLCMShort
}

func (lcm LCMShortHeader) NextLayerType() gopacket.LayerType {
	return getLCMLayerType(lcm.ChannelName)
}

func (lcm LCMShortHeader) LayerType() gopacket.LayerType {
	return LayerTypeLCMShort
}

func (lcm LCMShortHeader) LayerContents() []byte {
	return lcm.contents
}

func (lcm LCMShortHeader) LayerPayload() []byte {
	return lcm.payload
}

func (lcm LCMShortHeader) Payload() []byte {
	return lcm.payload
}
