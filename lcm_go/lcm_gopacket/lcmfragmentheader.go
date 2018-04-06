package lcm_gopacket

import (
	"encoding/binary"
	"github.com/google/gopacket"
)

type LCMFragmentHeader struct {
	Magic          uint32
	SequenceNumber uint32
	PayloadSize    uint32
	FragmentOffset uint32
	FragmentNumber uint16
	TotalFragments uint16
	ChannelName    string
	contents       []byte
	payload        []byte
}

var LayerTypeLCMFragment gopacket.LayerType

func init() {
	metadata := gopacket.LayerTypeMetadata{Name: "LCMFragmentHeader",
		Decoder: gopacket.DecodeFunc(decodeLCMFragmentHeader)}

	LayerTypeLCMFragment = gopacket.RegisterLayerType(1112, metadata)
}

func decodeLCMFragmentHeader(data []byte, p gopacket.PacketBuilder) error {
	lcmFragmentHeader := &LCMFragmentHeader{}

	err := lcmFragmentHeader.DecodeFromBytes(data, p)
	if err != nil {
		return err
	}

	p.AddLayer(lcmFragmentHeader)
	p.SetApplicationLayer(lcmFragmentHeader)

	return p.NextDecoder(lcmFragmentHeader.NextLayerType())
}

func (lcm *LCMFragmentHeader) DecodeFromBytes(data []byte, df gopacket.DecodeFeedback) error {
	var offset int = 0

	lcm.Magic = binary.BigEndian.Uint32(data[:offset+4])
	offset += 4

	lcm.SequenceNumber = binary.BigEndian.Uint32(data[offset : offset+4])
	offset += 4

	lcm.PayloadSize = binary.BigEndian.Uint32(data[offset : offset+4])
	offset += 4

	lcm.FragmentOffset = binary.BigEndian.Uint32(data[offset : offset+4])
	offset += 4

	lcm.FragmentNumber = binary.BigEndian.Uint16(data[offset : offset+2])
	offset += 2

	lcm.TotalFragments = binary.BigEndian.Uint16(data[offset : offset+2])
	offset += 2

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

func (lcm LCMFragmentHeader) CanDecode() gopacket.LayerClass {
	return LayerTypeLCMFragment
}

func (lcm LCMFragmentHeader) NextLayerType() gopacket.LayerType {
	return getLCMLayerType(lcm.ChannelName)
}

func (lcm LCMFragmentHeader) LayerType() gopacket.LayerType {
	return LayerTypeLCMFragment
}

func (lcm LCMFragmentHeader) LayerContents() []byte {
	return lcm.contents
}

func (lcm LCMFragmentHeader) LayerPayload() []byte {
	return lcm.payload
}

func (lcm LCMFragmentHeader) Payload() []byte {
	return lcm.payload
}
